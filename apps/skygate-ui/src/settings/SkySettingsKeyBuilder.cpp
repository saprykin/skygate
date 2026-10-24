#include "SkySettingsKeyBuilder.hpp"

namespace skygate::ui::internal {

QString SkySettingsKeyBuilder::key(const QString& name)
{
    return QStringLiteral("skyContext/%1").arg(name);
}

QString SkySettingsKeyBuilder::key(const char* name)
{
    return key(QString::fromUtf8(name));
}

QString SkySettingsKeyBuilder::nestedKey(const char* prefix, const char* name)
{
    return key(QStringLiteral("%1/%2").arg(QString::fromUtf8(prefix), QString::fromUtf8(name)));
}

QString SkySettingsKeyBuilder::appKey(const char* name)
{
    return QStringLiteral("app/%1").arg(QString::fromUtf8(name));
}

}  // namespace skygate::ui::internal
