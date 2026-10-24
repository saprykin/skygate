#include "SkyCatalogSourceRecordCodec.hpp"
#include "SkyCatalogSourceCachePaths.hpp"
#include "catalog/CatalogCompositionPolicyRules.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"
#include "settings/SkyCacheFiles.hpp"
#include "settings/SkySettingsValueCodecs.hpp"

#include <QLoggingCategory>
#include <QSettings>

#include <cstddef>
#include <string_view>

namespace skygate::ui::internal {
namespace {

// Settings keys of one durable source record. The names live in one set so the
// save, load, and eviction paths cannot drift apart. The two payload
// references are also read by SkyCatalogCollectionStore through
// SkyCatalogSourceRecordCodec::payloadPathKey()/binaryPayloadPathKey().
namespace RecordKey {
constexpr std::string_view kInstanceId = "instanceId";
constexpr std::string_view kDescriptorId = "descriptorId";
constexpr std::string_view kTitle = "title";
constexpr std::string_view kVersion = "version";
constexpr std::string_view kUrl = "url";
constexpr std::string_view kUrls = "urls";
constexpr std::string_view kRelatedDatasetUrls = "relatedDatasetUrls";
constexpr std::string_view kArchiveSelector = "archiveSelector";
constexpr std::string_view kSchemaHint = "schemaHint";
constexpr std::string_view kAttribution = "attribution";
constexpr std::string_view kPolicy = "policy";
constexpr std::string_view kEnabled = "enabled";
constexpr std::string_view kBundled = "bundled";
constexpr std::string_view kOrder = "order";
constexpr std::string_view kPayloadPath = "payloadPath";
constexpr std::string_view kBinaryPayloadPath = "binaryPayloadPath";
constexpr std::string_view kConstellationLineRows = "constellationLineRows";
constexpr std::string_view kConstellationAnchorGroupRows = "constellationAnchorGroupRows";
constexpr std::string_view kConstellationLineSchemaVersion = "constellationLineSchemaVersion";
constexpr std::string_view kConstellationCount = "constellationCount";
}  // namespace RecordKey

QString key(const std::string_view name)
{
    return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()));
}

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

QStringList evictedPayloadKeys()
{
    return QStringList{
        SkyCatalogSourceRecordCodec::payloadPathKey(),
        SkyCatalogSourceRecordCodec::binaryPayloadPathKey(),
        key(RecordKey::kConstellationLineRows),
        key(RecordKey::kConstellationAnchorGroupRows),
        key(RecordKey::kConstellationLineSchemaVersion),
        key(RecordKey::kConstellationCount),
    };
}

}  // namespace

QString SkyCatalogSourceRecordCodec::sourcesGroup()
{
    return QStringLiteral("catalogSources");
}

QString SkyCatalogSourceRecordCodec::payloadPathKey()
{
    return key(RecordKey::kPayloadPath);
}

QString SkyCatalogSourceRecordCodec::binaryPayloadPathKey()
{
    return key(RecordKey::kBinaryPayloadPath);
}

bool SkyCatalogSourceRecordCodec::isGenerationGroup(const QString& group)
{
    return generationOfGroup(group).has_value();
}

std::optional<quint64> SkyCatalogSourceRecordCodec::generationOfGroup(const QString& group)
{
    bool isGeneration = false;
    const quint64 generation = group.toULongLong(&isGeneration);
    if (!isGeneration) {
        return std::nullopt;
    }
    return generation;
}

QStringList SkyCatalogSourceRecordCodec::recordGroups(QSettings& settings)
{
    settings.beginGroup(sourcesGroup());
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    return groups;
}

bool SkyCatalogSourceRecordCodec::hasFlatRecords(QSettings& settings)
{
    for (const QString& group : recordGroups(settings)) {
        if (!isGenerationGroup(group)) {
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
    settings.setValue(key(RecordKey::kInstanceId), record.instanceId);
    settings.setValue(key(RecordKey::kDescriptorId), record.descriptorId);
    settings.setValue(key(RecordKey::kTitle), record.title);
    settings.setValue(key(RecordKey::kVersion), record.version);
    settings.setValue(key(RecordKey::kUrl), record.url);
    settings.setValue(key(RecordKey::kUrls), record.urls);
    settings.setValue(key(RecordKey::kRelatedDatasetUrls), record.relatedDatasetUrls);
    settings.setValue(key(RecordKey::kArchiveSelector), record.archiveSelector);
    settings.setValue(key(RecordKey::kSchemaHint), static_cast<int>(record.schemaHint));
    settings.setValue(key(RecordKey::kAttribution), record.attribution);
    settings.setValue(key(RecordKey::kPolicy), static_cast<int>(record.policy));
    settings.setValue(key(RecordKey::kEnabled), record.enabled);
    settings.setValue(key(RecordKey::kBundled), record.bundled);
    settings.setValue(key(RecordKey::kOrder), record.order);
    settings.setValue(payloadPathKey(), record.payload.isEmpty() ? QString() : rawPayloadPath);
    settings.setValue(binaryPayloadPathKey(), record.binaryPayload.isEmpty() ? QString() : binaryPayloadPath);
    settings.setValue(key(RecordKey::kConstellationLineRows), record.constellationLineRows);
    settings.setValue(key(RecordKey::kConstellationAnchorGroupRows), record.constellationAnchorGroupRows);
    settings.setValue(key(RecordKey::kConstellationLineSchemaVersion), record.constellationLineSchemaVersion);
    settings.setValue(key(RecordKey::kConstellationCount), static_cast<qulonglong>(record.constellationCount));
    settings.endGroup();
}

SkyCatalogSourceCacheRecord SkyCatalogSourceRecordCodec::loadRecord(
    QSettings& settings, const QString& group, QStringList& unreadablePayloadInstanceIds
)
{
    SkyCatalogSourceCacheRecord record;
    settings.beginGroup(group);
    record.instanceId = settings.value(key(RecordKey::kInstanceId)).toString();
    record.descriptorId = settings.value(key(RecordKey::kDescriptorId)).toString();
    record.title = settings.value(key(RecordKey::kTitle)).toString();
    record.version = settings.value(key(RecordKey::kVersion)).toString();
    record.url = settings.value(key(RecordKey::kUrl)).toString();
    record.urls = settings.value(key(RecordKey::kUrls)).toStringList();
    record.relatedDatasetUrls = settings.value(key(RecordKey::kRelatedDatasetUrls)).toStringList();
    record.archiveSelector = settings.value(key(RecordKey::kArchiveSelector)).toString();
    record.schemaHint = readCatalogSourceSchemaHint(settings, key(RecordKey::kSchemaHint));
    record.attribution = settings.value(key(RecordKey::kAttribution)).toString();
    record.policy = readCatalogCompositionPolicy(settings, key(RecordKey::kPolicy));
    record.enabled = SkySettingsValueCodecs::readBoolSetting(settings, key(RecordKey::kEnabled), true);
    record.bundled = SkySettingsValueCodecs::readBoolSetting(settings, key(RecordKey::kBundled), false);
    record.order = SkySettingsValueCodecs::readIntSetting(settings, key(RecordKey::kOrder), 0);
    record.constellationLineRows = settings.value(key(RecordKey::kConstellationLineRows)).toByteArray();
    record.constellationAnchorGroupRows = settings.value(key(RecordKey::kConstellationAnchorGroupRows)).toByteArray();
    record.constellationLineSchemaVersion =
        SkySettingsValueCodecs::readIntSetting(settings, key(RecordKey::kConstellationLineSchemaVersion), 0);
    record.constellationCount = static_cast<std::size_t>(SkySettingsValueCodecs::readULongLongSetting(
        settings, key(RecordKey::kConstellationCount), static_cast<qulonglong>(0)
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
