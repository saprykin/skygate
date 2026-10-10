#include "CatalogSourceFactsQuery.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace skygate::ui::tests {

bool CatalogSourceFactsQuery::containsInstanceId(
    const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId
)
{
    return std::any_of(
        snapshot.sources.begin(),
        snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
}

const SkySettingsStore::CatalogSourceCacheRecord* CatalogSourceFactsQuery::findRecord(
    const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId
)
{
    const auto it = std::find_if(
        snapshot.sources.begin(),
        snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    return it != snapshot.sources.end() ? &*it : nullptr;
}

QString
CatalogSourceFactsQuery::sidecarPath(const QString& directory, const QString& instanceId, const QString& extension)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString stem = QStringLiteral("catalog-source-") + QString::fromLatin1(digest.left(16));
    const QString pathKey =
        extension == QStringLiteral(".txt") ? QStringLiteral("payloadPath") : QStringLiteral("binaryPayloadPath");

    QSettings settings;
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList topLevelGroups = settings.childGroups();
    settings.endGroup();

    QStringList recordGroups;
    for (const QString& group : topLevelGroups) {
        settings.beginGroup(QStringLiteral("catalogSources/") + group);
        const QStringList nestedGroups = settings.childGroups();
        settings.endGroup();
        if (nestedGroups.isEmpty()) {
            recordGroups.push_back(QStringLiteral("catalogSources/") + group);
            continue;
        }
        for (const QString& nestedGroup : nestedGroups) {
            recordGroups.push_back(QStringLiteral("catalogSources/") + group + QLatin1Char('/') + nestedGroup);
        }
    }

    const QString directoryPath = QFileInfo(directory).absoluteFilePath();
    for (const QString& recordGroup : recordGroups) {
        if (!recordGroup.endsWith(QStringLiteral("/") + stem)) {
            continue;
        }
        settings.beginGroup(recordGroup);
        const QString path = settings.value(pathKey).toString();
        settings.endGroup();
        if (!path.isEmpty() && QFileInfo(path).absolutePath() == directoryPath) {
            return path;
        }
    }
    return {};
}

QString CatalogSourceFactsQuery::durableTitle(const QString& title)
{
    const QString savedSuffix = QStringLiteral(" (saved)");
    return title.endsWith(savedSuffix) ? title.left(title.size() - savedSuffix.size()) : title;
}

std::vector<skygate::ephemeris::ConstellationLineRef>
CatalogSourceFactsQuery::relatedLineRefs(const SkySettingsStore::CatalogSourceCacheRecord& record)
{
    return skygate::ui::internal::SkyContextCatalogCodec::parseConstellationLineRows(
        std::string_view(
            record.constellationLineRows.constData(), static_cast<std::size_t>(record.constellationLineRows.size())
        )
    );
}

bool CatalogSourceFactsQuery::relatedLineRefsContainHip(
    const std::vector<skygate::ephemeris::ConstellationLineRef>& lineRefs, const std::string& hipId
)
{
    return std::any_of(
        lineRefs.begin(), lineRefs.end(), [&hipId](const skygate::ephemeris::ConstellationLineRef& lineRef) {
            return lineRef.first == hipId || lineRef.second == hipId;
        }
    );
}

}  // namespace skygate::ui::tests
