#pragma once

#include "CatalogTestPayloads.hpp"
#include "SkySettingsStore.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

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

[[nodiscard]] inline SkySettingsStore::CatalogCacheSnapshot
sampleCatalogCacheSnapshot(const CatalogCacheSnapshotOptions& options = {})
{
    SkySettingsStore::CatalogCacheSnapshot snapshot;
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
