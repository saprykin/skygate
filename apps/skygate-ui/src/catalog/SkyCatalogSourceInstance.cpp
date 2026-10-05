#include "SkyCatalogSourceInstance.hpp"

#include <QCryptographicHash>
#include <QUuid>

namespace skygate::ui::internal {
namespace {

constexpr const char* kAllocatedInstanceIdPrefix = "src:";
constexpr const char* kLegacyPresetInstanceIdPrefix = "preset:";
constexpr const char* kLegacyCustomInstanceIdPrefix = "custom:";

QString legacyCustomInstanceId(const QString& urlText)
{
    const QByteArray digest = QCryptographicHash::hash(urlText.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(kLegacyCustomInstanceIdPrefix) + QString::fromLatin1(digest);
}

}  // namespace

QString SkyCatalogSourceInstance::allocateInstanceId()
{
    return QString::fromLatin1(kAllocatedInstanceIdPrefix) + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SkyCatalogSourceInstance::migratedLegacyInstanceId(const SkyCatalogSourceInstance& instance)
{
    if (!instance.descriptorId.isEmpty()) {
        return QString::fromLatin1(kLegacyPresetInstanceIdPrefix) + instance.descriptorId;
    }
    if (!instance.urls.isEmpty()) {
        return legacyCustomInstanceId(instance.urls.first().trimmed());
    }
    return {};
}

SkyCatalogSourceInstance SkyCatalogSourceInstance::fromDescriptor(const SkyCatalogSourceDescriptor& descriptor)
{
    SkyCatalogSourceInstance instance;
    instance.instanceId = allocateInstanceId();
    instance.descriptorId = descriptor.sourceId;
    instance.title = descriptor.title;
    instance.version = descriptor.version;
    instance.urls = descriptor.urls;
    instance.schemaHint = descriptor.schemaHint;
    instance.archiveSelector = descriptor.archiveSelector;
    instance.relatedDatasetUrls = descriptor.relatedDatasetUrls;
    instance.attribution = descriptor.attribution;
    return instance;
}

SkyCatalogSourceInstance SkyCatalogSourceInstance::createCustom(const QString& urlText, const QString& version)
{
    SkyCatalogSourceInstance instance;
    instance.instanceId = allocateInstanceId();
    instance.title = QStringLiteral("Downloaded");
    instance.version = version;
    instance.urls = QStringList{urlText.trimmed()};
    return instance;
}

bool SkyCatalogSourceInstance::isCustom() const noexcept
{
    return descriptorId.isEmpty();
}

}  // namespace skygate::ui::internal
