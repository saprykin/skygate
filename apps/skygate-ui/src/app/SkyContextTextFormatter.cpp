#include "SkyContextTextFormatter.hpp"

namespace skygate::ui::internal {

QString SkyContextTextFormatter::formatCoordinate(const double value)
{
    return QString::number(value, 'f', 6);
}

QString SkyContextTextFormatter::formatElevation(const double value)
{
    return QString::number(value, 'f', 1);
}

}  // namespace skygate::ui::internal
