#include "SkyCatalogCacheController.hpp"

#include "SkyContextControllerSupport.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogPayloadParser.hpp"

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
    while (normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)
           || normalizedSourceLabel.endsWith(compactSavedSuffix, Qt::CaseInsensitive)) {
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
    return std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()));
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

SkyCatalogCacheController::SkyCatalogCacheController(SkySettingsStore* settingsStore) : m_settingsStore(settingsStore)
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

SkyCatalogCacheRestoreResult
SkyCatalogCacheController::restore(const int catalogPresetIndex, const int deepSkyCatalogPresetIndex) const
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
    if (catalogPresetIndex != 0) {
        if (!cacheSnapshot->catalogBinaryPayload.isEmpty()
            && cacheSnapshot->catalogBinarySchemaVersion == skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion) {
            result.catalog = skygate::ephemeris::CatalogBinaryCodec::deserialize(cacheSnapshot->catalogBinaryPayload);
            if (result.catalog == nullptr) {
                qCWarning(skygateCatalogCacheLog).noquote()
                    << "Saved binary star catalog cache unreadable; falling back to payload parsing";
            }
        }
        if (result.catalog == nullptr && !cacheSnapshot->catalogPayload.isEmpty()) {
            auto restoredCatalogResult = parser.parseResult(payloadView(cacheSnapshot->catalogPayload));
            if (!restoredCatalogResult.isSuccess() || restoredCatalogResult.catalog == nullptr) {
                result.savedCatalogUnreadable = true;
                result.statusText = "Catalog: Saved cache unreadable, using bundled";
                qCWarning(skygateCatalogCacheLog).noquote()
                    << "Saved star catalog cache unreadable; using bundled catalog:"
                    << QString::fromStdString(restoredCatalogResult.errorDetail);
                return result;
            }

            result.catalog = std::move(restoredCatalogResult.catalog);
            result.requiresBinaryUpgrade = true;
        }
        if (result.catalog != nullptr) {
            result.catalogPayload = cacheSnapshot->catalogPayload;
            result.sourceLabel = savedLabel(cacheSnapshot->sourceLabel, "Saved");
            result.restored = true;
            qCInfo(skygateCatalogCacheLog).noquote()
                << "Saved star catalog cache restored:" << result.sourceLabel << "objects"
                << static_cast<qulonglong>(result.catalog->bodies().size()) << "starBytes"
                << cacheSnapshot->catalogPayload.size() << "binaryBytes" << cacheSnapshot->catalogBinaryPayload.size();
        }
    }

    if (deepSkyCatalogPresetIndex != 0) {
        if (!cacheSnapshot->deepSkyBinaryPayload.isEmpty()
            && cacheSnapshot->catalogBinarySchemaVersion == skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion) {
            result.deepSkyCatalog =
                skygate::ephemeris::CatalogBinaryCodec::deserialize(cacheSnapshot->deepSkyBinaryPayload);
            if (result.deepSkyCatalog == nullptr) {
                qCWarning(skygateCatalogCacheLog).noquote()
                    << "Saved binary deep-sky catalog cache unreadable; falling back to payload parsing";
            }
        }
        if (result.deepSkyCatalog == nullptr && !cacheSnapshot->deepSkyCatalogPayload.isEmpty()) {
            auto restoredDeepSkyResult = parser.parseResult(payloadView(cacheSnapshot->deepSkyCatalogPayload));
            if (restoredDeepSkyResult.isSuccess() && restoredDeepSkyResult.catalog != nullptr) {
                result.deepSkyCatalog = std::move(restoredDeepSkyResult.catalog);
                result.deepSkyObjectCount = restoredDeepSkyResult.diagnostics.parsedBodyCount;
                result.requiresBinaryUpgrade = true;
            } else {
                qCWarning(skygateCatalogCacheLog).noquote()
                    << "Saved deep-sky catalog cache unreadable; ignoring cache:"
                    << QString::fromStdString(restoredDeepSkyResult.errorDetail);
            }
        }
        if (result.deepSkyCatalog != nullptr) {
            result.deepSkyCatalogPayload = cacheSnapshot->deepSkyCatalogPayload;
            result.deepSkyObjectCount = result.deepSkyCatalog->bodies().size();
            result.deepSkySourceLabel = savedLabel(cacheSnapshot->deepSkySourceLabel, "Saved deep sky");
            result.restored = true;
            qCInfo(skygateCatalogCacheLog).noquote()
                << "Saved deep-sky catalog cache restored:" << result.deepSkySourceLabel << "objects"
                << static_cast<qulonglong>(result.deepSkyCatalog->bodies().size()) << "deepSkyBytes"
                << cacheSnapshot->deepSkyCatalogPayload.size() << "binaryBytes"
                << cacheSnapshot->deepSkyBinaryPayload.size();
        }
    }

    if (cacheSnapshot->constellationLineSchemaVersion
            >= SkyContextControllerConstants::kConstellationLineCacheSchemaVersion
        && !cacheSnapshot->constellationLineRows.isEmpty()) {
        auto parsedLineRefs =
            SkyContextCatalogCodec::parseConstellationLineRows(payloadView(cacheSnapshot->constellationLineRows));
        if (!parsedLineRefs.empty()) {
            result.constellationLineRefs = std::move(parsedLineRefs);
            if (!cacheSnapshot->constellationAnchorGroupRows.isEmpty()) {
                result.constellationAnchorGroups = SkyContextCatalogCodec::parseConstellationAnchorGroupRows(
                    payloadView(cacheSnapshot->constellationAnchorGroupRows)
                );
            }
            result.constellationCount = cacheSnapshot->constellationCount;
            result.restored = true;
            qCInfo(skygateCatalogCacheLog).noquote()
                << "Saved constellation line cache restored: segments"
                << static_cast<qulonglong>(result.constellationLineRefs.size()) << "labels"
                << static_cast<qulonglong>(result.constellationAnchorGroups.size());
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

void SkyCatalogCacheController::persist(const SkyCatalogCachePersistRequest& request) const
{
    if (m_settingsStore == nullptr || (request.catalogPayload.isEmpty() && request.deepSkyCatalogPayload.isEmpty())) {
        return;
    }

    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = request.sourceLabel;
    snapshot.deepSkySourceLabel = request.deepSkySourceLabel;
    snapshot.catalogPayload = request.catalogPayload;
    snapshot.deepSkyCatalogPayload = request.deepSkyCatalogPayload;
    snapshot.catalogBinaryPayload = request.catalogBinaryPayload;
    snapshot.deepSkyBinaryPayload = request.deepSkyBinaryPayload;
    snapshot.catalogBinarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    snapshot.constellationLineRows =
        SkyContextCatalogCodec::serializeConstellationLineRows(request.constellationLineRefs);
    snapshot.constellationAnchorGroupRows =
        SkyContextCatalogCodec::serializeConstellationAnchorGroupRows(request.constellationAnchorGroups);
    snapshot.constellationLineSchemaVersion = SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    snapshot.constellationCount = request.constellationCount;
    (void)m_settingsStore->saveCatalogCache(snapshot);
}

}  // namespace skygate::ui::internal
