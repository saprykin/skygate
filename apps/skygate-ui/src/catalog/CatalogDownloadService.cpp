#include "CatalogDownloadService.hpp"
#include "CatalogDownloadCancellation.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QLoggingCategory>
#include <QObject>
#include <QUrl>

#include <functional>
#include <memory>

namespace {

constexpr std::size_t kMaxDownloadedCatalogBytes = 128U << 20;

Q_LOGGING_CATEGORY(skygateCatalogDownloadLog, "skygate.catalog.download")

QStringList normalizedCandidateUrls(const QStringList& urlTexts)
{
    QStringList candidateUrls;
    candidateUrls.reserve(urlTexts.size());
    for (const QString& urlText : urlTexts) {
        const QString trimmedUrlText = urlText.trimmed();
        if (!trimmedUrlText.isEmpty()) {
            candidateUrls.push_back(trimmedUrlText);
        }
    }

    return candidateUrls;
}

}  // namespace

CatalogDownloadService::CatalogDownloadService(QNetworkAccessManager* networkAccessManager)
    : m_networkAccessManager(networkAccessManager)
{
}

std::shared_ptr<CatalogDownloadCancellation> CatalogDownloadService::downloadFirstSuccessfulFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CompletionHandler completionHandler
) const
{
    const auto cancellation = std::make_shared<CatalogDownloadCancellation>();
    const QStringList candidateUrls = normalizedCandidateUrls(urlTexts);
    if (candidateUrls.isEmpty()) {
        DownloadResult result;
        result.errorText = "Catalog: Invalid URL";
        qCWarning(skygateCatalogDownloadLog) << "Catalog download aborted: no valid source URLs";
        completionHandler(std::move(result));
        return cancellation;
    }

    if (m_networkAccessManager == nullptr) {
        DownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        qCWarning(skygateCatalogDownloadLog) << "Catalog download aborted: network unavailable";
        completionHandler(std::move(result));
        return cancellation;
    }

    qCInfo(skygateCatalogDownloadLog).noquote() << "Catalog download started: sources" << candidateUrls.size();
    const auto lastErrorText = std::make_shared<QString>("Catalog: Download failed");
    const auto tryDownloadNextUrl = std::make_shared<std::function<void(int)>>();
    const std::weak_ptr<std::function<void(int)>> weakTryDownloadNextUrl = tryDownloadNextUrl;
    *tryDownloadNextUrl = [this,
                           candidateUrls,
                           callbackContext,
                           statusHandler,
                           completionHandler,
                           lastErrorText,
                           cancellation,
                           weakTryDownloadNextUrl](const int index) {
        // The stored function refers to itself weakly; a pending reply holds
        // the strong copy that keeps the retry chain alive.
        const std::shared_ptr<std::function<void(int)>> downloadNextUrl = weakTryDownloadNextUrl.lock();

        if (cancellation->isCanceled()) {
            return;
        }

        if (index >= candidateUrls.size()) {
            DownloadResult result;
            result.errorText = *lastErrorText;
            completionHandler(std::move(result));
            return;
        }

        const QUrl url = QUrl::fromUserInput(candidateUrls[index]);
        if (!url.isValid() || url.scheme().isEmpty()) {
            *lastErrorText = QString("Catalog: Invalid source URL %1").arg(candidateUrls[index]);
            qCWarning(skygateCatalogDownloadLog).noquote()
                << "Skipping invalid catalog source URL" << candidateUrls[index];
            (*downloadNextUrl)(index + 1);
            return;
        }

        if (statusHandler) {
            statusHandler(QString("Catalog: Downloading %1...").arg(url.toString()));
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(300000);
        request.setRawHeader("User-Agent", "Skygate/1.0");
        request.setRawHeader(
            "Accept", "text/plain,text/csv,application/gzip,application/zip,application/octet-stream,*/*"
        );

        QNetworkReply* reply = m_networkAccessManager->get(request);
        cancellation->setActiveReply(reply);
        QObject* const contextObject =
            callbackContext != nullptr ? callbackContext : static_cast<QObject*>(m_networkAccessManager);

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            contextObject,
            [reply,
             index,
             candidateUrls,
             statusHandler,
             completionHandler,
             lastErrorText,
             cancellation,
             downloadNextUrl]() {
                if (cancellation->isCanceled()) {
                    // The operation was already marked terminal before the
                    // abort; another candidate or a completion must not
                    // follow from it.
                    return;
                }

                if (reply->error() != QNetworkReply::NoError) {
                    const int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    *lastErrorText =
                        QString("Catalog: Source %1 failed (%2, HTTP %3)")
                            .arg(candidateUrls[index], reply->errorString(), QString::number(httpStatusCode));
                    qCWarning(skygateCatalogDownloadLog).noquote() << "Catalog source failed" << candidateUrls[index]
                                                                   << reply->errorString() << "HTTP" << httpStatusCode;
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*downloadNextUrl)(index + 1);
                    return;
                }

                const QByteArray payload = reply->readAll();
                if (payload.isEmpty()) {
                    *lastErrorText = QString("Catalog: Source %1 returned empty data").arg(candidateUrls[index]);
                    qCWarning(skygateCatalogDownloadLog).noquote()
                        << "Catalog source returned empty data" << candidateUrls[index];
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*downloadNextUrl)(index + 1);
                    return;
                }

                if (static_cast<std::size_t>(payload.size()) > kMaxDownloadedCatalogBytes) {
                    *lastErrorText =
                        QString("Catalog: Source %1 file too large (max 128 MiB)").arg(candidateUrls[index]);
                    qCWarning(skygateCatalogDownloadLog).noquote()
                        << "Catalog source exceeded size limit" << candidateUrls[index] << payload.size();
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*downloadNextUrl)(index + 1);
                    return;
                }

                DownloadResult result;
                result.payload = payload;
                result.sourceUrl = candidateUrls[index];
                qCInfo(skygateCatalogDownloadLog).noquote()
                    << "Catalog download succeeded:" << candidateUrls[index] << "bytes" << payload.size();
                completionHandler(std::move(result));
            }
        );
        // Reply deletion must not depend on the callback context surviving.
        QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    };

    (*tryDownloadNextUrl)(0);
    return cancellation;
}
