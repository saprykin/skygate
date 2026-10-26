#include "SkyContextTextFormatter.hpp"

namespace skygate::ui::internal {

QString SkyContextTextFormatter::formatFixed(const double value, const int precision)
{
    return QString::number(value, 'f', precision);
}

QString SkyContextTextFormatter::formatCoordinate(const double value)
{
    return formatFixed(value, 6);
}

QString SkyContextTextFormatter::formatElevation(const double value)
{
    return formatFixed(value, 1);
}

}  // namespace skygate::ui::internal
