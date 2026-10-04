#include "SkyCatalogSourceInstance.hpp"

#include <QCryptographicHash>

namespace skygate::ui::internal {
namespace {

QString customInstanceIdForUrl(const QString& urlText)
{
    const QByteArray digest = QCryptographicHash::hash(urlText.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("custom:") + QString::fromLatin1(digest);
}

}  // namespace

SkyCatalogSourceInstance SkyCatalogSourceInstance::fromDescriptor(const SkyCatalogSourceDescriptor& descriptor)
{
    SkyCatalogSourceInstance instance;
    instance.instanceId = QStringLiteral("preset:") + descriptor.sourceId;
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
    instance.instanceId = customInstanceIdForUrl(urlText.trimmed());
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
