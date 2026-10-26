#include "SkyCatalogSourceCachePaths.hpp"
#include "settings/SkyCacheFiles.hpp"
#include "settings/SkySettingsKeyBuilder.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QSettings>

namespace skygate::ui::internal {
namespace {

constexpr int kIdDigestLength = 16;
constexpr char kSidecarFilePrefix[] = "catalog-source-";
constexpr char kGenerationFilePrefix[] = "catalog-source-g";

}  // namespace

QString SkyCatalogSourceCachePaths::defaultDirectory()
{
    return SkyCacheFiles::appDataFilePath(QStringLiteral("catalog-source-cache"));
}

QString SkyCatalogSourceCachePaths::directory(QSettings& settings)
{
    const QString configuredDirectory =
        settings.value(SkySettingsKeyBuilder::key(QStringLiteral("catalogCollectionCachePath")), defaultDirectory())
            .toString();
    return configuredDirectory.isEmpty() ? defaultDirectory() : configuredDirectory;
}

QString SkyCatalogSourceCachePaths::idDigest(const QString& instanceId)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest.left(kIdDigestLength));
}

QString SkyCatalogSourceCachePaths::fileStem(const QString& instanceId)
{
    return QString::fromLatin1(kSidecarFilePrefix) + idDigest(instanceId);
}

QString SkyCatalogSourceCachePaths::generationFileStem(const QString& instanceId, const quint64 generation)
{
    return QStringLiteral("%1%2-%3")
        .arg(QString::fromLatin1(kGenerationFilePrefix))
        .arg(generation)
        .arg(idDigest(instanceId));
}

QString SkyCatalogSourceCachePaths::rawSidecarPath(
    const QString& directory, const QString& instanceId, const quint64 generation
)
{
    return QDir(directory).filePath(generationFileStem(instanceId, generation) + QStringLiteral(".txt"));
}

QString SkyCatalogSourceCachePaths::binarySidecarPath(
    const QString& directory, const QString& instanceId, const quint64 generation
)
{
    return QDir(directory).filePath(generationFileStem(instanceId, generation) + QStringLiteral(".bin"));
}

QString SkyCatalogSourceCachePaths::sidecarEntryPattern()
{
    return QString::fromLatin1(kSidecarFilePrefix) + QLatin1Char('*');
}

std::optional<quint64> SkyCatalogSourceCachePaths::generationFromSidecarName(const QString& fileName)
{
    const QString prefix = QString::fromLatin1(kGenerationFilePrefix);
    if (!fileName.startsWith(prefix)) {
        return std::nullopt;
    }

    const int separator = fileName.indexOf(QLatin1Char('-'), prefix.size());
    if (separator < 0) {
        return std::nullopt;
    }

    bool ok = false;
    const quint64 generation = fileName.mid(prefix.size(), separator - prefix.size()).toULongLong(&ok);
    return ok ? std::optional<quint64>(generation) : std::nullopt;
}

}  // namespace skygate::ui::internal
