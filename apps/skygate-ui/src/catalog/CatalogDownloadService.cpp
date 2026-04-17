#include "CatalogDownloadService.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>

#include <functional>
#include <memory>

namespace {

constexpr std::size_t kMaxDownloadedCatalogBytes = 128U << 20;

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

void CatalogDownloadService::downloadFirstSuccessfulFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CompletionHandler completionHandler
) const
{
    const QStringList candidateUrls = normalizedCandidateUrls(urlTexts);
    if (candidateUrls.isEmpty()) {
        DownloadResult result;
        result.errorText = "Catalog: Invalid URL";
        completionHandler(std::move(result));
        return;
    }

    if (m_networkAccessManager == nullptr) {
        DownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    const auto lastErrorText = std::make_shared<QString>("Catalog: Download failed");
    const auto tryDownloadNextUrl = std::make_shared<std::function<void(int)>>();
    *tryDownloadNextUrl = [
        this,
        candidateUrls,
        callbackContext,
        statusHandler,
        completionHandler,
        lastErrorText,
        tryDownloadNextUrl
    ](const int index) {
        if (index >= candidateUrls.size()) {
            DownloadResult result;
            result.errorText = *lastErrorText;
            completionHandler(std::move(result));
            return;
        }

        const QUrl url = QUrl::fromUserInput(candidateUrls[index]);
        if (!url.isValid() || url.scheme().isEmpty()) {
            *lastErrorText = QString("Catalog: Invalid source URL %1").arg(candidateUrls[index]);
            (*tryDownloadNextUrl)(index + 1);
            return;
        }

        if (statusHandler) {
            statusHandler(QString("Catalog: Downloading %1 (%2/%3)").arg(
                url.toString(),
                QString::number(index + 1),
                QString::number(candidateUrls.size())
            ));
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(300000);
        request.setRawHeader("User-Agent", "Skygate/1.0");
        request.setRawHeader(
            "Accept",
            "text/plain,text/csv,application/gzip,application/zip,application/octet-stream,*/*"
        );

        QNetworkReply* reply = m_networkAccessManager->get(request);
        QObject* const contextObject = callbackContext != nullptr
            ? callbackContext
            : static_cast<QObject*>(m_networkAccessManager);

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            contextObject,
            [reply, index, candidateUrls, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl]() {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    const int httpStatusCode =
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    *lastErrorText = QString("Catalog: Source %1 failed (%2, HTTP %3)").arg(
                        candidateUrls[index],
                        reply->errorString(),
                        QString::number(httpStatusCode)
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                const QByteArray payload = reply->readAll();
                if (payload.isEmpty()) {
                    *lastErrorText = QString("Catalog: Source %1 returned empty data").arg(candidateUrls[index]);
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                if (static_cast<std::size_t>(payload.size()) > kMaxDownloadedCatalogBytes) {
                    *lastErrorText = QString("Catalog: Source %1 file too large (max 128 MiB)").arg(
                        candidateUrls[index]
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                DownloadResult result;
                result.payload = payload;
                result.sourceUrl = candidateUrls[index];
                completionHandler(std::move(result));
            }
        );
    };

    (*tryDownloadNextUrl)(0);
}
