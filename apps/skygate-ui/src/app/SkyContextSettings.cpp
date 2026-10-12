#include "SkyContextSettings.hpp"

namespace skygate::ui::internal {

QString SkyContextSettings::key(const QString& name)
{
    return QString("skyContext/%1").arg(name);
}

}  // namespace skygate::ui::internal
