#include "SkyCatalogCacheController.hpp"

#include "SkyContextControllerSupport.hpp"

#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include <QLoggingCategory>

#include <string_view>
#include <utility>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCacheLog, "skygate.catalog.cache")

QString stripSavedSuffixes(const QString& sourceLabel)
{
    QString normalizedSourceLabel = sourceLabel.trimmed();
    const QString spacedSavedSuffix = QStringLiteral(" (saved)");
    const QString compactSavedSuffix = QStringLiteral("(saved)");
    while (
        normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)
        || normalizedSourceLabel.endsWith(compactSavedSuffix, Qt::CaseInsensitive)
    ) {
        if (normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)) {
            normalizedSourceLabel.chop(spacedSavedSuffix.size());
        } else {
            normalizedSourceLabel.chop(compactSavedSuffix.size());
        }
        normalizedSourceLabel = normalizedSourceLabel.trimmed();
    }
    return normalizedSourceLabel;
}

std::string_view payloadView(const QByteArray& payload)
{
    return std::string_view(
        payload.constData(),
        static_cast<std::size_t>(payload.size())
    );
}

QString savedLabel(const QString& sourceLabel, const QString& fallbackLabel)
{
    QString normalizedSourceLabel = stripSavedSuffixes(sourceLabel);
    if (normalizedSourceLabel.isEmpty()) {
        normalizedSourceLabel = fallbackLabel;
    }
    return QString("%1 (saved)").arg(normalizedSourceLabel);
}

}  // namespace

SkyCatalogCacheController::SkyCatalogCacheController(SkySettingsStore* settingsStore)
    : m_settingsStore(settingsStore)
{
}

bool SkyCatalogCacheController::clearCatalogCache() const
{
    return m_settingsStore != nullptr && m_settingsStore->clearCatalogCache();
}

bool SkyCatalogCacheController::clearDeepSkyCatalogCache() const
{
    return m_settingsStore != nullptr && m_settingsStore->clearDeepSkyCatalogCache();
}

SkyCatalogCacheRestoreResult SkyCatalogCacheController::restore(
    const int catalogPresetIndex,
    const int deepSkyCatalogPresetIndex
) const
{
    SkyCatalogCacheRestoreResult result;
    if (m_settingsStore == nullptr) {
        return result;
    }
    if (catalogPresetIndex == 0 && deepSkyCatalogPresetIndex == 0) {
        return result;
    }

    const auto cacheSnapshot = m_settingsStore->loadCatalogCache();
    if (!cacheSnapshot.has_value()) {
        return result;
    }

    const skygate::ephemeris::CatalogPayloadParser parser;
    if (catalogPresetIndex != 0 && !cacheSnapshot->catalogPayload.isEmpty()) {
        auto restoredCatalogResult = parser.parseResult(
            payloadView(cacheSnapshot->catalogPayload)
        );
        if (!restoredCatalogResult.isSuccess() || restoredCatalogResult.catalog == nullptr) {
            result.savedCatalogUnreadable = true;
            result.statusText = "Catalog: Saved cache unreadable, using bundled";
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Saved star catalog cache unreadable; using bundled catalog:"
                << QString::fromStdString(restoredCatalogResult.errorDetail);
            return result;
        }

        result.catalogPayload = cacheSnapshot->catalogPayload;
        result.sourceLabel = savedLabel(cacheSnapshot->sourceLabel, "Saved");
        result.catalog = std::move(restoredCatalogResult.catalog);
        result.restored = true;
        qCInfo(skygateCatalogCacheLog).noquote()
            << "Saved star catalog cache restored:" << result.sourceLabel
            << "objects" << static_cast<qulonglong>(
                   restoredCatalogResult.diagnostics.selectedBodyCount
               )
            << "bytes" << cacheSnapshot->catalogPayload.size();
    }

    if (
        deepSkyCatalogPresetIndex != 0
        && !cacheSnapshot->deepSkyCatalogPayload.isEmpty()
    ) {
        auto restoredDeepSkyResult = parser.parseResult(
            payloadView(cacheSnapshot->deepSkyCatalogPayload)
        );
        if (restoredDeepSkyResult.isSuccess() && restoredDeepSkyResult.catalog != nullptr) {
            result.deepSkyCatalogPayload = cacheSnapshot->deepSkyCatalogPayload;
            result.deepSkyObjectCount = restoredDeepSkyResult.diagnostics.parsedBodyCount;
            result.deepSkySourceLabel = savedLabel(
                cacheSnapshot->deepSkySourceLabel,
                "Saved deep sky"
            );
            result.deepSkyCatalog = std::move(restoredDeepSkyResult.catalog);
            result.restored = true;
            qCInfo(skygateCatalogCacheLog).noquote()
                << "Saved deep-sky catalog cache restored:" << result.deepSkySourceLabel
                << "objects" << static_cast<qulonglong>(result.deepSkyObjectCount)
                << "bytes" << cacheSnapshot->deepSkyCatalogPayload.size();
        } else {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Saved deep-sky catalog cache unreadable; ignoring cache:"
                << QString::fromStdString(restoredDeepSkyResult.errorDetail);
        }
    }

    if (
        cacheSnapshot->constellationLineSchemaVersion
            >= SkyContextControllerConstants::kConstellationLineCacheSchemaVersion
        && !cacheSnapshot->constellationLineRows.isEmpty()
    ) {
        auto parsedLineRefs = SkyContextCatalogCodec::parseConstellationLineRows(
            payloadView(cacheSnapshot->constellationLineRows)
        );
        if (!parsedLineRefs.empty()) {
            result.constellationLineRefs = std::move(parsedLineRefs);
            if (!cacheSnapshot->constellationLabelRows.isEmpty()) {
                result.constellationLabelRefs = SkyContextCatalogCodec::parseConstellationLabelRows(
                    payloadView(cacheSnapshot->constellationLabelRows)
                );
            }
            result.constellationCount = cacheSnapshot->constellationCount;
            result.restored = true;
            qCInfo(skygateCatalogCacheLog).noquote()
                << "Saved constellation line cache restored: segments"
                << static_cast<qulonglong>(result.constellationLineRefs.size())
                << "labels" << static_cast<qulonglong>(result.constellationLabelRefs.size());
        } else {
            qCWarning(skygateCatalogCacheLog)
                << "Saved constellation line cache unreadable; clearing constellation refs";
        }
    } else if (cacheSnapshot->constellationLineSchemaVersion > 0) {
        result.resetConstellationLineRefs = true;
        result.restored = true;
    }

    return result;
}

void SkyCatalogCacheController::persist(
    const SkyCatalogCachePersistRequest& request
) const
{
    if (
        m_settingsStore == nullptr
        || (request.catalogPayload.isEmpty() && request.deepSkyCatalogPayload.isEmpty())
    ) {
        return;
    }

    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = request.sourceLabel;
    snapshot.deepSkySourceLabel = request.deepSkySourceLabel;
    snapshot.catalogPayload = request.catalogPayload;
    snapshot.deepSkyCatalogPayload = request.deepSkyCatalogPayload;
    snapshot.constellationLineRows = SkyContextCatalogCodec::serializeConstellationLineRows(
        request.constellationLineRefs
    );
    snapshot.constellationLabelRows = SkyContextCatalogCodec::serializeConstellationLabelRows(
        request.constellationLabelRefs
    );
    snapshot.constellationLineSchemaVersion =
        SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    snapshot.constellationCount = request.constellationCount;
    (void)m_settingsStore->saveCatalogCache(snapshot);
}

}  // namespace skygate::ui::internal
