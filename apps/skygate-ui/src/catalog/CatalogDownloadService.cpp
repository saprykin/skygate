#include "CatalogDownloadService.hpp"
#include "CatalogDownloadCancellation.hpp"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>
#include <QVariant>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

namespace {

constexpr std::size_t kMaxDownloadedCatalogBytes = 128U << 20U;
constexpr qint64 kCatalogReplyReadBufferBytes = 64 * 1024;
constexpr int kCatalogTransferTimeoutMs = 300000;

Q_LOGGING_CATEGORY(skygateCatalogDownloadLog, "skygate.catalog.download")

// Byte accounting for one candidate URL. `receivedBytes` counts every byte
// pulled off the reply, including a chunk dropped for crossing the cap.
struct CatalogDownloadAttempt final {
    QByteArray payload;
    std::size_t receivedBytes = 0;
    bool rejectedForSize = false;
};

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

std::optional<qlonglong> declaredContentLengthBytes(QNetworkReply* reply)
{
    const QVariant declaredLength = reply->header(QNetworkRequest::ContentLengthHeader);
    if (!declaredLength.isValid()) {
        return std::nullopt;
    }

    bool converted = false;
    const qlonglong declaredBytes = declaredLength.toLongLong(&converted);
    if (!converted) {
        return std::nullopt;
    }

    return declaredBytes;
}

// The declared length is only a hint: a reply that already says it is too
// large is rejected before its body is read. Missing or wrong headers fall
// through to the actual byte count.
bool rejectExcessiveDeclaredLength(QNetworkReply* reply, const QString& urlText, CatalogDownloadAttempt& attempt)
{
    if (attempt.rejectedForSize) {
        return true;
    }

    const std::optional<qlonglong> declaredBytes = declaredContentLengthBytes(reply);
    if (!declaredBytes.has_value() || *declaredBytes <= static_cast<qlonglong>(kMaxDownloadedCatalogBytes)) {
        return false;
    }

    attempt.rejectedForSize = true;
    qCWarning(skygateCatalogDownloadLog).noquote()
        << "Catalog source declared size over limit" << urlText << *declaredBytes;
    if (reply->isRunning()) {
        reply->abort();
    }
    return true;
}

// Returns false when the chunk would push the payload past the cap; the
// chunk is dropped and the attempt is marked rejected.
bool appendChunkWithinBudget(CatalogDownloadAttempt& attempt, const QByteArray& chunk)
{
    const auto incomingBytes = static_cast<std::size_t>(chunk.size());
    if (incomingBytes > kMaxDownloadedCatalogBytes - attempt.receivedBytes) {
        attempt.receivedBytes += incomingBytes;
        attempt.rejectedForSize = true;
        return false;
    }

    attempt.payload.append(chunk);
    attempt.receivedBytes += incomingBytes;
    return true;
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
        request.setTransferTimeout(kCatalogTransferTimeoutMs);
        request.setRawHeader("User-Agent", "Skygate/1.0");
        request.setRawHeader(
            "Accept", "text/plain,text/csv,application/gzip,application/zip,application/octet-stream,*/*"
        );

        QNetworkReply* reply = m_networkAccessManager->get(request);
        // Qt stops pulling from the socket once this many bytes wait unread,
        // so unread data stays bounded between readyRead signals.
        reply->setReadBufferSize(kCatalogReplyReadBufferBytes);
        cancellation->setActiveReply(reply);
        QObject* const contextObject =
            callbackContext != nullptr ? callbackContext : static_cast<QObject*>(m_networkAccessManager);

        const QString urlText = candidateUrls[index];
        const auto attempt = std::make_shared<CatalogDownloadAttempt>();

        QObject::connect(
            reply, &QNetworkReply::metaDataChanged, contextObject, [reply, urlText, attempt, cancellation]() {
                if (cancellation->isCanceled()) {
                    return;
                }
                rejectExcessiveDeclaredLength(reply, urlText, *attempt);
            }
        );

        QObject::connect(reply, &QNetworkReply::readyRead, contextObject, [reply, urlText, attempt, cancellation]() {
            if (cancellation->isCanceled() || attempt->rejectedForSize) {
                return;
            }
            if (rejectExcessiveDeclaredLength(reply, urlText, *attempt)) {
                return;
            }

            const QByteArray chunk = reply->readAll();
            if (chunk.isEmpty()) {
                return;
            }
            if (!appendChunkWithinBudget(*attempt, chunk)) {
                qCWarning(skygateCatalogDownloadLog).noquote()
                    << "Catalog source exceeded size limit" << urlText << attempt->receivedBytes;
                reply->abort();
            }
        });

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            contextObject,
            [reply,
             index,
             urlText,
             statusHandler,
             completionHandler,
             lastErrorText,
             cancellation,
             attempt,
             downloadNextUrl]() {
                if (cancellation->isCanceled()) {
                    // The operation was already marked terminal before the
                    // abort; another candidate or a completion must not
                    // follow from it.
                    return;
                }

                const auto reportSizeRejection = [&]() {
                    *lastErrorText = QString("Catalog: Source %1 file too large (max 128 MiB)").arg(urlText);
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*downloadNextUrl)(index + 1);
                };

                if (attempt->rejectedForSize) {
                    reportSizeRejection();
                    return;
                }

                if (reply->error() != QNetworkReply::NoError) {
                    const int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    *lastErrorText = QString("Catalog: Source %1 failed (%2, HTTP %3)")
                                         .arg(urlText, reply->errorString(), QString::number(httpStatusCode));
                    qCWarning(skygateCatalogDownloadLog).noquote()
                        << "Catalog source failed" << urlText << reply->errorString() << "HTTP" << httpStatusCode;
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*downloadNextUrl)(index + 1);
                    return;
                }

                if (rejectExcessiveDeclaredLength(reply, urlText, *attempt)) {
                    reportSizeRejection();
                    return;
                }

                const QByteArray remainingChunk = reply->readAll();
                if (!remainingChunk.isEmpty() && !appendChunkWithinBudget(*attempt, remainingChunk)) {
                    qCWarning(skygateCatalogDownloadLog).noquote()
                        << "Catalog source exceeded size limit" << urlText << attempt->receivedBytes;
                    reportSizeRejection();
                    return;
                }

                if (attempt->payload.isEmpty()) {
                    *lastErrorText = QString("Catalog: Source %1 returned empty data").arg(urlText);
                    qCWarning(skygateCatalogDownloadLog).noquote() << "Catalog source returned empty data" << urlText;
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*downloadNextUrl)(index + 1);
                    return;
                }

                DownloadResult result;
                result.payload = std::move(attempt->payload);
                result.sourceUrl = urlText;
                qCInfo(skygateCatalogDownloadLog).noquote()
                    << "Catalog download succeeded:" << urlText << "bytes" << result.payload.size();
                completionHandler(std::move(result));
            }
        );
        // Reply deletion must not depend on the callback context surviving.
        QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    };

    (*tryDownloadNextUrl)(0);
    return cancellation;
}
