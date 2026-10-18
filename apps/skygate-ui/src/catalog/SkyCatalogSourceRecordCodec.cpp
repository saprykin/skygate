#include "SkyCatalogSourceRecordCodec.hpp"
#include "SkyCatalogSourceCachePaths.hpp"
#include "catalog/CatalogCompositionPolicyRules.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "settings/SkyCacheFiles.hpp"
#include "settings/SkySettingsValueCodecs.hpp"

#include <QLoggingCategory>
#include <QSettings>

#include <cstddef>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCacheLog, "skygate.catalog.cache")
Q_LOGGING_CATEGORY(skygateSettingsLog, "skygate.settings")

// A stored policy only means something when it names a policy this build knows.
// An unknown value falls back to the merge policy instead of handing
// composition a value no rule can classify.
skygate::ephemeris::CatalogCompositionPolicy readCatalogCompositionPolicy(QSettings& settings, const QString& key)
{
    const int storedValue = SkySettingsValueCodecs::readIntSetting(
        settings, key, static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
    );
    const auto storedPolicy = static_cast<skygate::ephemeris::CatalogCompositionPolicy>(storedValue);
    if (skygate::ephemeris::CatalogCompositionPolicyRules::isKnown(storedPolicy)) {
        return storedPolicy;
    }

    qCWarning(skygateCatalogCacheLog).noquote()
        << "Invalid catalog composition policy" << key << "value" << storedValue << "- using fallback: merge policy";
    return skygate::ephemeris::CatalogCompositionPolicy::Merge;
}

// A stored schema hint only means something when it names a schema this build
// knows. A missing or garbled value falls back to "no hint" so detection
// decides, instead of failing the source with a synthetic hint mismatch.
skygate::ephemeris::CatalogSourceType readCatalogSourceSchemaHint(QSettings& settings, const QString& key)
{
    const auto noHint = skygate::ephemeris::CatalogSourceType::Unknown;
    const auto storedType = static_cast<skygate::ephemeris::CatalogSourceType>(
        SkySettingsValueCodecs::readIntSetting(settings, key, static_cast<int>(noHint))
    );
    if (storedType == noHint || skygate::ephemeris::CatalogSchemaRegistry::find(storedType) != nullptr) {
        return storedType;
    }

    qCWarning(skygateSettingsLog).noquote() << "Invalid catalog source schema hint" << key << "value"
                                            << settings.value(key).toString() << "- using fallback: no schema hint";
    return noHint;
}

// A referenced sidecar is expected to exist once its path is stored. A missing
// or unreadable file is corrupt committed data, not a deliberately evicted
// payload, which keeps no reference at all.
struct CatalogSourcePayloadReadResult final {
    QByteArray payload;
    bool unreadable = false;
};

CatalogSourcePayloadReadResult readCatalogSourceFile(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    const SkyCacheFiles::ReadResult result = SkyCacheFiles::readFile(path);
    if (result.state == SkyCacheFiles::ReadState::Absent) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Referenced catalog source payload is missing" << path;
        return {.unreadable = true};
    }
    if (result.state == SkyCacheFiles::ReadState::Unreadable) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog source cache" << path << result.errorString;
        return {.unreadable = true};
    }
    return {.payload = result.payload};
}

QString payloadPathKey()
{
    return QStringLiteral("payloadPath");
}

QString binaryPayloadPathKey()
{
    return QStringLiteral("binaryPayloadPath");
}

QStringList evictedPayloadKeys()
{
    return QStringList{
        payloadPathKey(),
        binaryPayloadPathKey(),
        QStringLiteral("constellationLineRows"),
        QStringLiteral("constellationAnchorGroupRows"),
        QStringLiteral("constellationLineSchemaVersion"),
        QStringLiteral("constellationCount"),
    };
}

}  // namespace

QString SkyCatalogSourceRecordCodec::sourcesGroup()
{
    return QStringLiteral("catalogSources");
}

bool SkyCatalogSourceRecordCodec::hasFlatRecords(QSettings& settings)
{
    settings.beginGroup(sourcesGroup());
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    for (const QString& group : groups) {
        bool isGenerationGroup = false;
        group.toULongLong(&isGenerationGroup);
        if (!isGenerationGroup) {
            return true;
        }
    }
    return false;
}

QString SkyCatalogSourceRecordCodec::sourceGroup(const QString& parentGroup, const QString& instanceId)
{
    return parentGroup + QLatin1Char('/') + SkyCatalogSourceCachePaths::fileStem(instanceId);
}

void SkyCatalogSourceRecordCodec::saveRecord(
    QSettings& settings,
    const QString& group,
    const SkyCatalogSourceCacheRecord& record,
    const QString& rawPayloadPath,
    const QString& binaryPayloadPath
)
{
    settings.beginGroup(group);
    settings.setValue(QStringLiteral("instanceId"), record.instanceId);
    settings.setValue(QStringLiteral("descriptorId"), record.descriptorId);
    settings.setValue(QStringLiteral("title"), record.title);
    settings.setValue(QStringLiteral("version"), record.version);
    settings.setValue(QStringLiteral("url"), record.url);
    settings.setValue(QStringLiteral("urls"), record.urls);
    settings.setValue(QStringLiteral("relatedDatasetUrls"), record.relatedDatasetUrls);
    settings.setValue(QStringLiteral("archiveSelector"), record.archiveSelector);
    settings.setValue(QStringLiteral("schemaHint"), static_cast<int>(record.schemaHint));
    settings.setValue(QStringLiteral("attribution"), record.attribution);
    settings.setValue(QStringLiteral("policy"), static_cast<int>(record.policy));
    settings.setValue(QStringLiteral("enabled"), record.enabled);
    settings.setValue(QStringLiteral("bundled"), record.bundled);
    settings.setValue(QStringLiteral("order"), record.order);
    settings.setValue(payloadPathKey(), record.payload.isEmpty() ? QString() : rawPayloadPath);
    settings.setValue(binaryPayloadPathKey(), record.binaryPayload.isEmpty() ? QString() : binaryPayloadPath);
    settings.setValue(QStringLiteral("constellationLineRows"), record.constellationLineRows);
    settings.setValue(QStringLiteral("constellationAnchorGroupRows"), record.constellationAnchorGroupRows);
    settings.setValue(QStringLiteral("constellationLineSchemaVersion"), record.constellationLineSchemaVersion);
    settings.setValue(QStringLiteral("constellationCount"), static_cast<qulonglong>(record.constellationCount));
    settings.endGroup();
}

SkyCatalogSourceCacheRecord SkyCatalogSourceRecordCodec::loadRecord(
    QSettings& settings, const QString& group, QStringList& unreadablePayloadInstanceIds
)
{
    SkyCatalogSourceCacheRecord record;
    settings.beginGroup(group);
    record.instanceId = settings.value(QStringLiteral("instanceId")).toString();
    record.descriptorId = settings.value(QStringLiteral("descriptorId")).toString();
    record.title = settings.value(QStringLiteral("title")).toString();
    record.version = settings.value(QStringLiteral("version")).toString();
    record.url = settings.value(QStringLiteral("url")).toString();
    record.urls = settings.value(QStringLiteral("urls")).toStringList();
    record.relatedDatasetUrls = settings.value(QStringLiteral("relatedDatasetUrls")).toStringList();
    record.archiveSelector = settings.value(QStringLiteral("archiveSelector")).toString();
    record.schemaHint = readCatalogSourceSchemaHint(settings, QStringLiteral("schemaHint"));
    record.attribution = settings.value(QStringLiteral("attribution")).toString();
    record.policy = readCatalogCompositionPolicy(settings, QStringLiteral("policy"));
    record.enabled = SkySettingsValueCodecs::readBoolSetting(settings, QStringLiteral("enabled"), true);
    record.bundled = SkySettingsValueCodecs::readBoolSetting(settings, QStringLiteral("bundled"), false);
    record.order = SkySettingsValueCodecs::readIntSetting(settings, QStringLiteral("order"), 0);
    record.constellationLineRows = settings.value(QStringLiteral("constellationLineRows")).toByteArray();
    record.constellationAnchorGroupRows = settings.value(QStringLiteral("constellationAnchorGroupRows")).toByteArray();
    record.constellationLineSchemaVersion =
        SkySettingsValueCodecs::readIntSetting(settings, QStringLiteral("constellationLineSchemaVersion"), 0);
    record.constellationCount = static_cast<std::size_t>(SkySettingsValueCodecs::readULongLongSetting(
        settings, QStringLiteral("constellationCount"), static_cast<qulonglong>(0)
    ));
    const QString rawPath = settings.value(payloadPathKey()).toString();
    const QString binaryPath = settings.value(binaryPayloadPathKey()).toString();
    settings.endGroup();

    const CatalogSourcePayloadReadResult rawPayload = readCatalogSourceFile(rawPath);
    const CatalogSourcePayloadReadResult binaryPayload = readCatalogSourceFile(binaryPath);
    record.payload = rawPayload.payload;
    record.binaryPayload = binaryPayload.payload;
    if ((rawPayload.unreadable || binaryPayload.unreadable) && !record.instanceId.isEmpty()
        && !unreadablePayloadInstanceIds.contains(record.instanceId)) {
        unreadablePayloadInstanceIds.push_back(record.instanceId);
    }
    return record;
}

void SkyCatalogSourceRecordCodec::evictPayload(QSettings& settings, const QString& group)
{
    for (const QString& key : evictedPayloadKeys()) {
        settings.remove(group + QLatin1Char('/') + key);
    }
}

}  // namespace skygate::ui::internal
