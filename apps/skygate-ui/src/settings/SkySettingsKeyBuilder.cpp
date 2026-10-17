#include "SkySettingsKeyBuilder.hpp"

namespace skygate::ui::internal {

QString SkySettingsKeyBuilder::key(const QString& name)
{
    return QString("skyContext/%1").arg(name);
}

}  // namespace skygate::ui::internal
