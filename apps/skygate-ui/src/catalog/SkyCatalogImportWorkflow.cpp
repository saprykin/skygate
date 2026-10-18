#include "SkyCatalogImportWorkflow.hpp"
#include "CatalogCoordinator.hpp"
#include "CatalogParseOptions.hpp"
#include "SkyCatalogSourceImportResult.hpp"
#include "SkyCatalogText.hpp"
#include "SkyConstellationLineImportResult.hpp"
#include "SkyPayloadView.hpp"
#include "catalog/CatalogIdentity.hpp"
#include "catalog/stellarium/StellariumConstellationParser.hpp"

#include <QLoggingCategory>

#include <utility>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

SkyCatalogImportWorkflow::SkyCatalogImportWorkflow(QNetworkAccessManager* networkAccessManager)
    : m_catalogCoordinator(std::make_unique<CatalogCoordinator>(networkAccessManager))
{
}

SkyCatalogImportWorkflow::~SkyCatalogImportWorkflow() = default;

bool SkyCatalogImportWorkflow::isAvailable() const noexcept
{
    return m_catalogCoordinator != nullptr;
}

void SkyCatalogImportWorkflow::downloadSource(
    const std::shared_ptr<CatalogDownloadCancellation>& cancellation,
    const SkyCatalogSourceInstance& source,
    const skygate::ephemeris::CatalogCompositionPolicy policy,
    QObject* callbackContext,
    StatusHandler statusHandler,
    SourceCompletionHandler completionHandler
) const
{
    if (m_catalogCoordinator == nullptr) {
        SkyCatalogSourceImportResult result;
        result.sourceLabel = source.title;
        result.sourceId = source.instanceId;
        result.sourceVersion = source.version;
        result.errorText = SkyCatalogText::networkUnavailable();
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadCatalogFromUrls(
        cancellation,
        source.urls,
        CatalogParseOptions{
            .archiveMember = source.archiveSelector,
            .schemaHint = source.schemaHint,
        },
        callbackContext,
        std::move(statusHandler),
        [source,
         policy,
         completionHandler = std::move(completionHandler)](CatalogCoordinator::DownloadResult downloadResult) mutable {
            SkyCatalogSourceImportResult result;
            result.payload = std::move(downloadResult.payload);
            result.catalog = std::move(downloadResult.catalog);
            result.diagnostics = downloadResult.diagnostics;
            result.sourceLabel = source.title;
            result.sourceId = source.instanceId;
            result.sourceVersion = source.version;
            result.sourceUrl = std::move(downloadResult.sourceUrl);
            result.errorText = std::move(downloadResult.errorText);

            if (result.catalog == nullptr || policy != skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly) {
                completionHandler(std::move(result));
                return;
            }

            const auto bodies = result.catalog->bodies();
            result.foundObjectCount = skygate::ephemeris::CatalogIdentity::countDeepSkyObjects(bodies);
            if (result.foundObjectCount == 0U) {
                result.catalog.reset();
                result.errorText = SkyCatalogText::emptyDeepSkyCatalog();
            }
            completionHandler(std::move(result));
        }
    );
}

void SkyCatalogImportWorkflow::downloadConstellationLines(
    const std::shared_ptr<CatalogDownloadCancellation>& cancellation,
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    ConstellationCompletionHandler completionHandler
) const
{
    if (m_catalogCoordinator == nullptr) {
        SkyConstellationLineImportResult result;
        result.statusSuffix = SkyCatalogText::missingConstellationData(SkyCatalogText::networkUnavailable());
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadRawDataFromUrls(
        cancellation,
        urlTexts,
        callbackContext,
        std::move(statusHandler),
        [completionHandler = std::move(completionHandler)](CatalogCoordinator::RawDownloadResult lineResult) mutable {
            SkyConstellationLineImportResult result;
            if (lineResult.payload.isEmpty()) {
                const QString reason = lineResult.errorText.isEmpty() ? QString("unavailable") : lineResult.errorText;
                result.statusSuffix = SkyCatalogText::missingConstellationData(reason);
                qCWarning(skygateCatalogParseLog).noquote()
                    << "Constellation line download unavailable; no bundled fallback:" << reason;
                completionHandler(std::move(result));
                return;
            }

            const skygate::ephemeris::StellariumConstellationParser parser;
            auto parsedData = parser.parse(SkyPayloadView::of(lineResult.payload));
            if (parsedData.lineRefs.empty()) {
                QString payloadPreview = QString::fromUtf8(lineResult.payload.left(120)).simplified();
                if (payloadPreview.isEmpty()) {
                    payloadPreview = "<empty>";
                }
                result.statusSuffix =
                    SkyCatalogText::missingConstellationData(QString("parse failed: %1").arg(payloadPreview));
                qCWarning(skygateCatalogParseLog).noquote()
                    << "Constellation line parse failed; no bundled fallback. Payload preview:" << payloadPreview;
                completionHandler(std::move(result));
                return;
            }

            result.lineRefs = std::move(parsedData.lineRefs);
            result.anchorGroups = std::move(parsedData.anchorGroups);
            result.constellationCount = parsedData.constellationCount;
            result.statusSuffix =
                QString("%1 segments").arg(QString::number(static_cast<qulonglong>(result.lineRefs.size())));
            qCInfo(skygateCatalogParseLog).noquote()
                << "Constellation lines parsed:" << static_cast<qulonglong>(result.lineRefs.size()) << "segments"
                << static_cast<qulonglong>(result.anchorGroups.size()) << "labels"
                << static_cast<qulonglong>(result.constellationCount) << "constellations";
            completionHandler(std::move(result));
        }
    );
}

}  // namespace skygate::ui::internal
