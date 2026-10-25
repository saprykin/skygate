#include "SkyLegacyCatalogCacheStore.hpp"
#include "SkyCacheFiles.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsValueCodecs.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <optional>

namespace skygate::ui::internal {
namespace {

// Legacy two-slot catalog cache files, kept under their original names so an
// older installation still restores.
constexpr const char* kCatalogCacheFileName = "catalog-cache-v2.txt";
constexpr const char* kDeepSkyCatalogCacheFileName = "deep-sky-catalog-cache-v1.txt";
constexpr const char* kCatalogBinaryCacheFileName = "catalog-cache-v3.bin";
constexpr const char* kDeepSkyBinaryCatalogCacheFileName = "deep-sky-catalog-cache-v2.bin";

QString defaultCatalogCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kCatalogCacheFileName));
}

QString defaultDeepSkyCatalogCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kDeepSkyCatalogCacheFileName));
}

QString defaultCatalogBinaryCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kCatalogBinaryCacheFileName));
}

QString defaultDeepSkyBinaryCatalogCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kDeepSkyBinaryCatalogCacheFileName));
}

// A missing legacy cache file is first use, while a file that exists but cannot
// be opened is a storage problem worth reporting.
void loadCachePayload(const QString& path, const QString& logNoun, QByteArray& payload)
{
    if (path.isEmpty()) {
        return;
    }

    const SkyCacheFiles::ReadResult result = SkyCacheFiles::readFile(path);
    if (result.state == SkyCacheFiles::ReadState::Ok) {
        payload = result.payload;
        return;
    }
    if (result.state == SkyCacheFiles::ReadState::Unreadable) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Failed to open" << logNoun << path << result.errorString;
    }
}

}  // namespace

std::optional<SkyCatalogLegacyCacheSnapshot> SkyLegacyCatalogCacheStore::load()
{
    QSettings settings;
    const QString configuredPath =
        settings.value(SkySettingsKeyBuilder::key("catalogCachePath"), defaultCatalogCachePath()).toString();

    SkyCatalogLegacyCacheSnapshot snapshot;
    loadCachePayload(configuredPath, QStringLiteral("star catalog cache"), snapshot.catalogPayload);

    const QString configuredDeepSkyPath =
        settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), defaultDeepSkyCatalogCachePath())
            .toString();
    loadCachePayload(configuredDeepSkyPath, QStringLiteral("deep-sky catalog cache"), snapshot.deepSkyCatalogPayload);

    const QString configuredBinaryPath = defaultCatalogBinaryCachePath();
    loadCachePayload(configuredBinaryPath, QStringLiteral("binary star catalog cache"), snapshot.catalogBinaryPayload);

    const QString configuredDeepSkyBinaryPath = defaultDeepSkyBinaryCatalogCachePath();
    loadCachePayload(
        configuredDeepSkyBinaryPath, QStringLiteral("binary deep-sky catalog cache"), snapshot.deepSkyBinaryPayload
    );

    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()
        && snapshot.catalogBinaryPayload.isEmpty() && snapshot.deepSkyBinaryPayload.isEmpty()) {
        return std::nullopt;
    }

    if (!snapshot.catalogPayload.isEmpty()) {
        snapshot.sourceLabel =
            settings.value(SkySettingsKeyBuilder::key("catalogSourceLabel"), QString("Saved")).toString();
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        snapshot.deepSkySourceLabel =
            settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogSourceLabel"), QString("Saved deep sky"))
                .toString();
    }
    snapshot.constellationLineRows =
        settings.value(SkySettingsKeyBuilder::key("catalogConstellationLineRefs")).toByteArray();
    snapshot.constellationAnchorGroupRows =
        settings.value(SkySettingsKeyBuilder::key("catalogConstellationAnchorGroups")).toByteArray();
    snapshot.constellationLineSchemaVersion = SkySettingsValueCodecs::readIntSetting(
        settings, SkySettingsKeyBuilder::key("catalogConstellationLineSchemaVersion"), 0
    );
    snapshot.constellationCount = static_cast<std::size_t>(SkySettingsValueCodecs::readULongLongSetting(
        settings, SkySettingsKeyBuilder::key("catalogConstellationCount"), static_cast<qulonglong>(0)
    ));
    snapshot.catalogBinarySchemaVersion =
        SkySettingsValueCodecs::readIntSetting(settings, SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"), 0);
    qCInfo(skygateCatalogCacheLog).noquote()
        << "Catalog cache loaded: starBytes" << snapshot.catalogPayload.size() << "deepSkyBytes"
        << snapshot.deepSkyCatalogPayload.size() << "starBinaryBytes" << snapshot.catalogBinaryPayload.size()
        << "deepSkyBinaryBytes" << snapshot.deepSkyBinaryPayload.size();
    return snapshot;
}

bool SkyLegacyCatalogCacheStore::clearStarCache()
{
    QSettings settings;
    const QString configuredPath =
        settings.value(SkySettingsKeyBuilder::key("catalogCachePath"), defaultCatalogCachePath()).toString();
    const QString defaultPath = defaultCatalogCachePath();

    QStringList cachePaths;
    SkyCacheFiles::appendUniquePath(cachePaths, configuredPath);
    SkyCacheFiles::appendUniquePath(cachePaths, defaultPath);
    SkyCacheFiles::appendUniquePath(cachePaths, defaultCatalogBinaryCachePath());

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries =
            defaultDir.entryList(QStringList{"catalog-cache*"}, QDir::Files | QDir::Readable);
        for (const QString& entryName : matchingEntries) {
            SkyCacheFiles::appendUniquePath(cachePaths, defaultDir.filePath(entryName));
        }
    }

    const bool removedAllCacheFiles = SkyCacheFiles::removeFiles(cachePaths, QStringLiteral("catalog cache"));

    settings.remove(SkySettingsKeyBuilder::key("catalogCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("catalogBinaryCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"));
    settings.remove(SkySettingsKeyBuilder::key("catalogSourceLabel"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationLineRefs"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationAnchorGroups"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationLineSchemaVersion"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationCount"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to clear star catalog cache settings";
        return false;
    }
    if (removedAllCacheFiles) {
        qCInfo(skygateCatalogCacheLog).noquote() << "Star catalog cache cleared: files" << cachePaths.size();
    }
    return removedAllCacheFiles;
}

bool SkyLegacyCatalogCacheStore::clearDeepSkyCache()
{
    QSettings settings;
    QStringList cachePaths;
    SkyCacheFiles::appendUniquePath(
        cachePaths,
        settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), defaultDeepSkyCatalogCachePath())
            .toString()
    );
    SkyCacheFiles::appendUniquePath(cachePaths, defaultDeepSkyCatalogCachePath());
    SkyCacheFiles::appendUniquePath(cachePaths, defaultDeepSkyBinaryCatalogCachePath());

    const bool removedAllCacheFiles = SkyCacheFiles::removeFiles(cachePaths, QStringLiteral("catalog cache"));
    settings.remove(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("deepSkyBinaryCatalogCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("deepSkyCatalogSourceLabel"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to clear deep-sky catalog cache settings";
        return false;
    }
    if (removedAllCacheFiles) {
        qCInfo(skygateCatalogCacheLog).noquote() << "Deep-sky catalog cache cleared: files" << cachePaths.size();
    }
    return removedAllCacheFiles;
}

}  // namespace skygate::ui::internal
