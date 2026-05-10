#include "SkyCatalogImportWorkflow.hpp"

#include "CatalogCoordinator.hpp"

#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QLoggingCategory>

#include <algorithm>
#include <span>
#include <string_view>
#include <utility>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

std::size_t countDeepSkyObjects(
    const std::span<const skygate::ephemeris::CelestialBody> bodies
)
{
    return static_cast<std::size_t>(std::count_if(
        bodies.begin(),
        bodies.end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject;
        }
    ));
}

std::string_view payloadView(const QByteArray& payload)
{
    return std::string_view(
        payload.constData(),
        static_cast<std::size_t>(payload.size())
    );
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

void SkyCatalogImportWorkflow::downloadCatalog(
    const QStringList& urlTexts,
    const QString& sourceLabel,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CatalogCompletionHandler completionHandler
) const
{
    if (m_catalogCoordinator == nullptr) {
        SkyCatalogImportResult result;
        result.sourceLabel = sourceLabel;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadCatalogFromUrls(
        urlTexts,
        callbackContext,
        std::move(statusHandler),
        [sourceLabel, completionHandler = std::move(completionHandler)](
            CatalogCoordinator::DownloadResult downloadResult
        ) mutable {
            SkyCatalogImportResult result;
            result.payload = std::move(downloadResult.payload);
            result.catalog = std::move(downloadResult.catalog);
            result.diagnostics = downloadResult.diagnostics;
            result.sourceLabel = sourceLabel;
            result.errorText = std::move(downloadResult.errorText);
            completionHandler(std::move(result));
        }
    );
}

void SkyCatalogImportWorkflow::downloadDeepSkyCatalog(
    const QStringList& urlTexts,
    const QString& sourceLabel,
    QObject* callbackContext,
    StatusHandler statusHandler,
    DeepSkyCompletionHandler completionHandler
) const
{
    if (m_catalogCoordinator == nullptr) {
        SkyDeepSkyCatalogImportResult result;
        result.sourceLabel = sourceLabel;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadCatalogFromUrls(
        urlTexts,
        callbackContext,
        std::move(statusHandler),
        [sourceLabel, completionHandler = std::move(completionHandler)](
            CatalogCoordinator::DownloadResult downloadResult
        ) mutable {
            SkyDeepSkyCatalogImportResult result;
            result.payload = std::move(downloadResult.payload);
            result.catalog = std::move(downloadResult.catalog);
            result.sourceLabel = sourceLabel;
            result.errorText = std::move(downloadResult.errorText);

            if (result.catalog == nullptr) {
                completionHandler(std::move(result));
                return;
            }

            const auto bodies = result.catalog->bodies();
            result.foundObjectCount = downloadResult.diagnostics.parsedBodyCount > 0U
                ? downloadResult.diagnostics.parsedBodyCount
                : countDeepSkyObjects(bodies);
            if (countDeepSkyObjects(bodies) == 0U) {
                result.catalog.reset();
                result.errorText = "Catalog: Downloaded deep-sky catalog contains no DSOs";
            }
            completionHandler(std::move(result));
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
        result.statusSuffix = "fallback default (Catalog: Network unavailable)";
        completionHandler(std::move(result));
        return;
    }

    m_catalogCoordinator->downloadRawDataFromUrls(
        urlTexts,
        callbackContext,
        std::move(statusHandler),
        [completionHandler = std::move(completionHandler)](
            CatalogCoordinator::RawDownloadResult lineResult
        ) mutable {
            SkyConstellationLineImportResult result;
            if (lineResult.payload.isEmpty()) {
                const QString reason = lineResult.errorText.isEmpty()
                    ? QString("unavailable")
                    : lineResult.errorText;
                result.statusSuffix = QString("fallback default (%1)").arg(reason);
                qCWarning(skygateCatalogParseLog).noquote()
                    << "Constellation line download unavailable; using minimal fallback:" << reason;
                completionHandler(std::move(result));
                return;
            }

            const skygate::ephemeris::StellariumConstellationParser parser;
            auto parsedData = parser.parse(payloadView(lineResult.payload));
            if (parsedData.lineRefs.empty()) {
                QString payloadPreview = QString::fromUtf8(
                    lineResult.payload.left(120)
                ).simplified();
                if (payloadPreview.isEmpty()) {
                    payloadPreview = "<empty>";
                }
                result.statusSuffix = QString("fallback default (parse failed: %1)").arg(
                    payloadPreview
                );
                qCWarning(skygateCatalogParseLog).noquote()
                    << "Constellation line parse failed; using minimal fallback. Payload preview:"
                    << payloadPreview;
                completionHandler(std::move(result));
                return;
            }

            result.lineRefs = std::move(parsedData.lineRefs);
            result.labelRefs = std::move(parsedData.labelRefs);
            result.constellationCount = parsedData.constellationCount;
            result.statusSuffix = QString("%1 segments").arg(
                QString::number(static_cast<qulonglong>(result.lineRefs.size()))
            );
            qCInfo(skygateCatalogParseLog).noquote()
                << "Constellation lines parsed:" << static_cast<qulonglong>(result.lineRefs.size())
                << "segments" << static_cast<qulonglong>(result.labelRefs.size())
                << "labels" << static_cast<qulonglong>(result.constellationCount)
                << "constellations";
            completionHandler(std::move(result));
        }
    );
}

}  // namespace skygate::ui::internal
