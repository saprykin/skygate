#include "SkyCatalogImportWorkflow.hpp"
#include "CatalogCoordinator.hpp"
#include "catalog/CatalogIdentity.hpp"
#include "catalog/stellarium/StellariumConstellationParser.hpp"

#include <QLoggingCategory>

#include <string_view>
#include <utility>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

std::string_view payloadView(const QByteArray& payload)
{
    return std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()));
}

}  // namespace

bool SkyConstellationLineImportResult::hasCustomLines() const noexcept
{
    return !lineRefs.empty();
}

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
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadCatalogFromUrls(
        source.urls,
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
            result.foundObjectCount = result.diagnostics.parsedBodyCount > 0U
                                          ? result.diagnostics.parsedBodyCount
                                          : skygate::ephemeris::CatalogIdentity::countDeepSkyObjects(bodies);
            if (skygate::ephemeris::CatalogIdentity::countDeepSkyObjects(bodies) == 0U) {
                result.catalog.reset();
                result.errorText = "Catalog: Downloaded deep-sky catalog contains no DSOs";
            }
            completionHandler(std::move(result));
        }
    );
}

void SkyCatalogImportWorkflow::downloadCatalog(
    const SkyCatalogSourceInstance& source,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CatalogCompletionHandler completionHandler
) const
{
    downloadSource(
        source,
        skygate::ephemeris::CatalogCompositionPolicy::Merge,
        callbackContext,
        std::move(statusHandler),
        [completionHandler = std::move(completionHandler)](SkyCatalogSourceImportResult result) mutable {
            SkyCatalogImportResult legacyResult;
            legacyResult.payload = std::move(result.payload);
            legacyResult.catalog = std::move(result.catalog);
            legacyResult.diagnostics = result.diagnostics;
            legacyResult.sourceLabel = std::move(result.sourceLabel);
            legacyResult.sourceId = std::move(result.sourceId);
            legacyResult.sourceVersion = std::move(result.sourceVersion);
            legacyResult.sourceUrl = std::move(result.sourceUrl);
            legacyResult.errorText = std::move(result.errorText);
            completionHandler(std::move(legacyResult));
        }
    );
}

void SkyCatalogImportWorkflow::downloadDeepSkyCatalog(
    const SkyCatalogSourceInstance& source,
    QObject* callbackContext,
    StatusHandler statusHandler,
    DeepSkyCompletionHandler completionHandler
) const
{
    downloadSource(
        source,
        skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
        callbackContext,
        std::move(statusHandler),
        [completionHandler = std::move(completionHandler)](SkyCatalogSourceImportResult result) mutable {
            SkyDeepSkyCatalogImportResult legacyResult;
            legacyResult.payload = std::move(result.payload);
            legacyResult.catalog = std::move(result.catalog);
            legacyResult.foundObjectCount = result.foundObjectCount;
            legacyResult.sourceLabel = std::move(result.sourceLabel);
            legacyResult.sourceId = std::move(result.sourceId);
            legacyResult.sourceVersion = std::move(result.sourceVersion);
            legacyResult.sourceUrl = std::move(result.sourceUrl);
            legacyResult.errorText = std::move(result.errorText);
            completionHandler(std::move(legacyResult));
        }
    );
}

void SkyCatalogImportWorkflow::downloadConstellationLines(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    ConstellationCompletionHandler completionHandler
) const
{
    if (m_catalogCoordinator == nullptr) {
        SkyConstellationLineImportResult result;
        result.statusSuffix = "no constellation data (Catalog: Network unavailable)";
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadRawDataFromUrls(
        urlTexts,
        callbackContext,
        std::move(statusHandler),
        [completionHandler = std::move(completionHandler)](CatalogCoordinator::RawDownloadResult lineResult) mutable {
            SkyConstellationLineImportResult result;
            if (lineResult.payload.isEmpty()) {
                const QString reason = lineResult.errorText.isEmpty() ? QString("unavailable") : lineResult.errorText;
                result.statusSuffix = QString("no constellation data (%1)").arg(reason);
                qCWarning(skygateCatalogParseLog).noquote()
                    << "Constellation line download unavailable; no bundled fallback:" << reason;
                completionHandler(std::move(result));
                return;
            }

            const skygate::ephemeris::StellariumConstellationParser parser;
            auto parsedData = parser.parse(payloadView(lineResult.payload));
            if (parsedData.lineRefs.empty()) {
                QString payloadPreview = QString::fromUtf8(lineResult.payload.left(120)).simplified();
                if (payloadPreview.isEmpty()) {
                    payloadPreview = "<empty>";
                }
                result.statusSuffix = QString("no constellation data (parse failed: %1)").arg(payloadPreview);
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
