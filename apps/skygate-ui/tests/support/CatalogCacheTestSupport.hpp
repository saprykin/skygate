#pragma once

#include "CatalogTestPayloads.hpp"
#include "catalog/SkyCatalogLegacyCacheSnapshot.hpp"
#include "settings/SkyCacheFiles.hpp"
#include "settings/SkyPersistedStateSnapshot.hpp"
#include "settings/SkySettingsKeyBuilder.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QString>

#include <cstddef>

namespace skygate::ui::tests {

struct CatalogCacheSnapshotOptions final {
    QString sourceLabel = QStringLiteral("Saved");
    QString deepSkySourceLabel = QStringLiteral("Saved OpenNGC");
    QByteArray constellationLineRows = "hip_1|hip_2\n";
    QByteArray constellationAnchorGroupRows = "Demo|hip_1,hip_2\n";
    int constellationLineSchemaVersion = 4;
    std::size_t constellationCount = 1;
};

[[nodiscard]] inline SkyCatalogLegacyCacheSnapshot
sampleCatalogCacheSnapshot(const CatalogCacheSnapshotOptions& options = {})
{
    SkyCatalogLegacyCacheSnapshot snapshot;
    snapshot.sourceLabel = options.sourceLabel;
    snapshot.catalogPayload = sampleHygCsvPayload();
    snapshot.deepSkySourceLabel = options.deepSkySourceLabel;
    snapshot.deepSkyCatalogPayload = sampleCompactOpenNgcCsvPayload();
    snapshot.constellationLineRows = options.constellationLineRows;
    snapshot.constellationAnchorGroupRows = options.constellationAnchorGroupRows;
    snapshot.constellationLineSchemaVersion = options.constellationLineSchemaVersion;
    snapshot.constellationCount = options.constellationCount;
    return snapshot;
}

// Arranges the legacy two-slot cache an older installation leaves behind: the
// payload files under their original names and the settings keys that reference
// them. Production only reads and clears this storage, so the suites write the
// restored state directly.
[[nodiscard]] inline bool seedLegacyCatalogCache(const SkyCatalogLegacyCacheSnapshot& snapshot)
{
    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()) {
        return false;
    }

    // The file names an older installation wrote; the reader still resolves
    // them from the application data directory.
    const QString starCacheFileName = QStringLiteral("catalog-cache-v2.txt");
    const QString deepSkyCacheFileName = QStringLiteral("deep-sky-catalog-cache-v1.txt");
    const QString starBinaryCacheFileName = QStringLiteral("catalog-cache-v3.bin");
    const QString deepSkyBinaryCacheFileName = QStringLiteral("deep-sky-catalog-cache-v2.bin");
    QSettings settings;
    const auto defaultPath = [](const QString& fileName) {
        return skygate::ui::internal::SkyCacheFiles::appDataFilePath(fileName);
    };
    const QString configuredPath =
        settings
            .value(
                skygate::ui::internal::SkySettingsKeyBuilder::key("catalogCachePath"), defaultPath(starCacheFileName)
            )
            .toString();
    const QString configuredDeepSkyPath =
        settings
            .value(
                skygate::ui::internal::SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"),
                defaultPath(deepSkyCacheFileName)
            )
            .toString();
    const auto writePayload = [](const QString& path, const QByteArray& payload) {
        if (path.isEmpty() || payload.isEmpty()) {
            return true;
        }
        return skygate::ui::internal::SkyCacheFiles::writeFileAtomically(
            path, payload, QStringLiteral("catalog cache")
        );
    };

    if (!writePayload(configuredPath, snapshot.catalogPayload)
        || !writePayload(configuredDeepSkyPath, snapshot.deepSkyCatalogPayload)
        || !writePayload(defaultPath(starBinaryCacheFileName), snapshot.catalogBinaryPayload)
        || !writePayload(defaultPath(deepSkyBinaryCacheFileName), snapshot.deepSkyBinaryPayload)) {
        return false;
    }

    const QString configuredBinaryPath = defaultPath(starBinaryCacheFileName);
    const QString configuredDeepSkyBinaryPath = defaultPath(deepSkyBinaryCacheFileName);
    settings.setValue(
        skygate::ui::internal::SkySettingsKeyBuilder::key("version"), SkyPersistedStateSnapshot::kSettingsVersion
    );
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(skygate::ui::internal::SkySettingsKeyBuilder::key("catalogCachePath"), configuredPath);
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), configuredDeepSkyPath
        );
    }
    if (!snapshot.catalogBinaryPayload.isEmpty()) {
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogBinaryCachePath"), configuredBinaryPath
        );
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"),
            snapshot.catalogBinarySchemaVersion
        );
    }
    if (!snapshot.deepSkyBinaryPayload.isEmpty()) {
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("deepSkyBinaryCatalogCachePath"),
            configuredDeepSkyBinaryPath
        );
    }
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogSourceLabel"), snapshot.sourceLabel
        );
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogConstellationLineRefs"),
            snapshot.constellationLineRows
        );
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogConstellationAnchorGroups"),
            snapshot.constellationAnchorGroupRows
        );
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogConstellationLineSchemaVersion"),
            snapshot.constellationLineSchemaVersion
        );
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("catalogConstellationCount"),
            static_cast<qulonglong>(snapshot.constellationCount)
        );
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(
            skygate::ui::internal::SkySettingsKeyBuilder::key("deepSkyCatalogSourceLabel"), snapshot.deepSkySourceLabel
        );
    }
    settings.sync();
    return settings.status() == QSettings::NoError;
}

// The collection cache publishes every save as a new generation. A test that
// injects a failure into one specific staged write needs the committed
// generation number and the sidecar name the writer will use next.
[[nodiscard]] inline QString catalogCollectionManifestFilePath(const QString& directory)
{
    return QDir(directory).filePath(QStringLiteral("catalog-collection.manifest"));
}

[[nodiscard]] inline quint64 committedCatalogCollectionGeneration(const QString& directory)
{
    QFile manifestFile(catalogCollectionManifestFilePath(directory));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return 0;
    }

    const QRegularExpression generationPattern(QStringLiteral(R"((?:^|\n)generation=(\d+))"));
    const QRegularExpressionMatch match = generationPattern.match(QString::fromUtf8(manifestFile.readAll()));
    return match.hasMatch() ? match.captured(1).toULongLong() : 0;
}

[[nodiscard]] inline QString stagedCatalogSourceSidecarPath(
    const QString& directory, const quint64 generation, const QString& instanceId, const QString& extension
)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QDir(directory).filePath(QStringLiteral("catalog-source-g%1-%2%3")
                                        .arg(generation)
                                        .arg(QString::fromLatin1(digest.left(16)))
                                        .arg(extension));
}

}  // namespace skygate::ui::tests
