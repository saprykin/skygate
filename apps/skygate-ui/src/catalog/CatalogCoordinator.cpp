#include "CatalogCoordinator.hpp"

#include "catalog/CatalogDownloadCancellation.hpp"
#include "catalog/CatalogDownloadService.hpp"
#include "catalog/CatalogPayloadParseService.hpp"
#include "catalog/SkyCatalogText.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QObject>

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

using skygate::ephemeris::CatalogLoadResult;
using skygate::ui::skygateCatalogParseLog;
using skygate::ui::internal::SkyCatalogImportStatus;
using skygate::ui::internal::SkyCatalogText;

namespace {

QString catalogLoadErrorDescription(const CatalogLoadResult::ErrorCode errorCode)
{
    const std::string_view description = CatalogLoadResult::describeErrorCode(errorCode);
    return QString::fromLatin1(description.data(), static_cast<qsizetype>(description.size()));
}

QString catalogLoadErrorText(const CatalogLoadResult& loadResult, const QString& sourceUrl)
{
    QString description = catalogLoadErrorDescription(loadResult.errorCode);
    if (!loadResult.errorDetail.empty()) {
        description = QString("%1 (%2)").arg(description, QString::fromStdString(loadResult.errorDetail));
    }

    return SkyCatalogText::sourceParseFailed(sourceUrl, description);
}
}  // namespace

CatalogCoordinator::CatalogCoordinator(QNetworkAccessManager* networkAccessManager)
    : m_downloadService(std::make_unique<CatalogDownloadService>(networkAccessManager)),
      m_parseService(std::make_unique<CatalogPayloadParseService>()), m_networkAccessManager(networkAccessManager)
{
}

CatalogCoordinator::~CatalogCoordinator() = default;

void CatalogCoordinator::downloadCatalogFromUrls(
    const std::shared_ptr<CatalogDownloadCancellation>& cancellation,
    const QStringList& urlTexts,
    CatalogParseOptions parseOptions,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CompletionHandler completionHandler
) const
{
    if (m_downloadService == nullptr || m_parseService == nullptr) {
        DownloadResult result;
        result.errorText = SkyCatalogText::networkUnavailable();
        completionHandler(std::move(result));
        return;
    }

    m_downloadService->downloadFirstSuccessfulFromUrls(
        cancellation,
        urlTexts,
        callbackContext,
        [statusHandler](const QString& statusText) {
            if (statusHandler) {
                statusHandler(
                    SkyCatalogImportStatus{
                        .text = statusText,
                        .phase = SkyCatalogImportStatus::Phase::Downloading,
                    }
                );
            }
        },
        [this, cancellation, callbackContext, statusHandler, parseOptions = std::move(parseOptions), completionHandler](
            CatalogDownloadService::DownloadResult downloadResult
        ) mutable {
            if (downloadResult.payload.isEmpty()) {
                DownloadResult result;
                result.errorText =
                    downloadResult.errorText.isEmpty() ? SkyCatalogText::downloadFailed() : downloadResult.errorText;
                result.sourceUrl = downloadResult.sourceUrl;
                completionHandler(std::move(result));
                return;
            }

            if (statusHandler) {
                statusHandler(
                    SkyCatalogImportStatus{
                        .text = SkyCatalogText::processingDownloadedCatalog(),
                        .phase = SkyCatalogImportStatus::Phase::Processing,
                    }
                );
                // A synchronous cancellation from the status notification
                // must stop the parse from being dispatched.
                if (cancellation->isCanceled()) {
                    return;
                }
            }

            QObject* const contextObject =
                callbackContext != nullptr ? callbackContext : static_cast<QObject*>(m_networkAccessManager);
            if (contextObject == nullptr) {
                DownloadResult result;
                result.errorText = SkyCatalogText::callbackContextUnavailable();
                result.sourceUrl = downloadResult.sourceUrl;
                completionHandler(std::move(result));
                return;
            }

            const QString sourceUrl = downloadResult.sourceUrl;
            const QByteArray payload = downloadResult.payload;
            m_parseService->parseAsync(
                payload,
                contextObject,
                std::move(parseOptions),
                [statusHandler](const std::size_t parsedObjectCount) {
                    if (!statusHandler) {
                        return;
                    }

                    statusHandler(
                        SkyCatalogImportStatus{
                            .text = SkyCatalogText::processingParsedObjects(parsedObjectCount),
                            .phase = SkyCatalogImportStatus::Phase::Processing,
                        }
                    );
                },
                [sourceUrl, payload, statusHandler, completionHandler = std::move(completionHandler)](
                    CatalogLoadResult loadResult
                ) mutable {
                    if (!loadResult.isSuccess()) {
                        DownloadResult result;
                        result.errorText = catalogLoadErrorText(loadResult, sourceUrl);
                        result.sourceUrl = sourceUrl;
                        qCWarning(skygateCatalogParseLog).noquote() << result.errorText;
                        if (statusHandler) {
                            statusHandler(
                                SkyCatalogImportStatus{
                                    .text = result.errorText,
                                    .phase = SkyCatalogImportStatus::Phase::Failed,
                                }
                            );
                        }
                        completionHandler(std::move(result));
                        return;
                    }

                    DownloadResult result;
                    result.payload = payload;
                    result.catalog = std::move(loadResult.catalog);
                    result.diagnostics = loadResult.diagnostics;
                    result.sourceUrl = sourceUrl;
                    completionHandler(std::move(result));
                }
            );
        }
    );
}

void CatalogCoordinator::downloadRawDataFromUrls(
    const std::shared_ptr<CatalogDownloadCancellation>& cancellation,
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    RawCompletionHandler completionHandler
) const
{
    if (m_downloadService == nullptr) {
        RawDownloadResult result;
        result.errorText = SkyCatalogText::networkUnavailable();
        completionHandler(std::move(result));
        return;
    }

    m_downloadService->downloadFirstSuccessfulFromUrls(
        cancellation,
        urlTexts,
        callbackContext,
        [statusHandler](const QString& statusText) {
            if (statusHandler) {
                statusHandler(
                    SkyCatalogImportStatus{
                        .text = statusText,
                        .phase = SkyCatalogImportStatus::Phase::Downloading,
                    }
                );
            }
        },
        [completionHandler](CatalogDownloadService::DownloadResult downloadResult) mutable {
            RawDownloadResult result;
            result.payload = std::move(downloadResult.payload);
            result.sourceUrl = std::move(downloadResult.sourceUrl);
            result.errorText = std::move(downloadResult.errorText);
            completionHandler(std::move(result));
        }
    );
}
