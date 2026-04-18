#include "CatalogCoordinator.hpp"

#include "catalog/CatalogDownloadService.hpp"
#include "catalog/CatalogPayloadParseService.hpp"

#include <QLocale>
#include <QNetworkAccessManager>
#include <QObject>

#include <cstddef>
#include <memory>
#include <utility>

namespace {

QString catalogLoadErrorDescription(const skygate::ephemeris::CatalogLoadErrorCode errorCode)
{
    switch (errorCode) {
    case skygate::ephemeris::CatalogLoadErrorCode::None:
        return "Unknown parse failure";
    case skygate::ephemeris::CatalogLoadErrorCode::EmptyInput:
        return "empty payload";
    case skygate::ephemeris::CatalogLoadErrorCode::UnsupportedFormat:
        return "unsupported format";
    case skygate::ephemeris::CatalogLoadErrorCode::InvalidRows:
        return "invalid pipe-row catalog";
    case skygate::ephemeris::CatalogLoadErrorCode::MissingRequiredColumns:
        return "missing required HYG columns";
    case skygate::ephemeris::CatalogLoadErrorCode::InvalidHygCsv:
        return "invalid HYG CSV payload";
    case skygate::ephemeris::CatalogLoadErrorCode::InvalidGzipData:
        return "invalid gzip payload";
    case skygate::ephemeris::CatalogLoadErrorCode::InvalidZipData:
        return "invalid ZIP payload";
    case skygate::ephemeris::CatalogLoadErrorCode::NoBodies:
        return "catalog contains no usable bodies";
    }

    return "unknown parse failure";
}

QString catalogLoadErrorText(
    const skygate::ephemeris::CatalogLoadResult& loadResult,
    const QString& sourceUrl
)
{
    QString description = catalogLoadErrorDescription(loadResult.errorCode);
    if (!loadResult.errorDetail.empty()) {
        description = QString("%1 (%2)").arg(
            description,
            QString::fromStdString(loadResult.errorDetail)
        );
    }

    return QString("Catalog: Source %1 parse failed: %2").arg(sourceUrl, description);
}
}  // namespace

CatalogCoordinator::CatalogCoordinator(QNetworkAccessManager* networkAccessManager)
    : m_downloadService(std::make_unique<CatalogDownloadService>(networkAccessManager))
    , m_parseService(std::make_unique<CatalogPayloadParseService>())
    , m_networkAccessManager(networkAccessManager)
{
}

CatalogCoordinator::~CatalogCoordinator() = default;

void CatalogCoordinator::downloadCatalogFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CompletionHandler completionHandler
) const
{
    if (m_downloadService == nullptr || m_parseService == nullptr) {
        DownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_downloadService->downloadFirstSuccessfulFromUrls(
        urlTexts,
        callbackContext,
        statusHandler,
        [this, callbackContext, statusHandler, completionHandler](CatalogDownloadService::DownloadResult downloadResult) mutable {
            if (downloadResult.payload.isEmpty()) {
                DownloadResult result;
                result.errorText = downloadResult.errorText.isEmpty()
                    ? QString("Catalog: Download failed")
                    : downloadResult.errorText;
                completionHandler(std::move(result));
                return;
            }

            if (statusHandler) {
                statusHandler("Catalog: Processing downloaded catalog...");
            }

            QObject* const contextObject = callbackContext != nullptr
                ? callbackContext
                : static_cast<QObject*>(m_networkAccessManager);
            if (contextObject == nullptr) {
                DownloadResult result;
                result.errorText = "Catalog: Callback context unavailable";
                completionHandler(std::move(result));
                return;
            }

            const QString sourceUrl = downloadResult.sourceUrl;
            m_parseService->parseAsync(
                downloadResult.payload,
                contextObject,
                {},
                [statusHandler](const std::size_t parsedObjectCount) {
                    if (!statusHandler) {
                        return;
                    }

                    const QString countText =
                        QLocale::system().toString(static_cast<qulonglong>(parsedObjectCount));
                    statusHandler(QString("Catalog: Processing... %1 objects parsed").arg(countText));
                },
                [sourceUrl, statusHandler, completionHandler = std::move(completionHandler)](
                    skygate::ephemeris::CatalogLoadResult loadResult
                ) mutable {
                    if (!loadResult.isSuccess()) {
                        DownloadResult result;
                        result.errorText = catalogLoadErrorText(loadResult, sourceUrl);
                        if (statusHandler) {
                            statusHandler(result.errorText);
                        }
                        completionHandler(std::move(result));
                        return;
                    }

                    DownloadResult result;
                    result.catalog = std::move(loadResult.catalog);
                    result.diagnostics = loadResult.diagnostics;
                    completionHandler(std::move(result));
                }
            );
        }
    );
}

void CatalogCoordinator::downloadRawDataFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    RawCompletionHandler completionHandler
) const
{
    if (m_downloadService == nullptr) {
        RawDownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_downloadService->downloadFirstSuccessfulFromUrls(
        urlTexts,
        callbackContext,
        statusHandler,
        [completionHandler](CatalogDownloadService::DownloadResult downloadResult) mutable {
            RawDownloadResult result;
            result.payload = std::move(downloadResult.payload);
            result.sourceUrl = std::move(downloadResult.sourceUrl);
            result.errorText = std::move(downloadResult.errorText);
            completionHandler(std::move(result));
        }
    );
}
