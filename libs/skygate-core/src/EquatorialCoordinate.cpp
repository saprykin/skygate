#include "EquatorialCoordinate.hpp"

#include <cmath>

namespace skygate::core {

bool EquatorialCoordinate::isFinite() const noexcept
{
    return std::isfinite(rightAscensionHours) && std::isfinite(declinationDeg);
}

bool EquatorialCoordinate::isValid() const noexcept
{
    return isFinite() && rightAscensionHours >= kRightAscensionMinHours && rightAscensionHours < kRightAscensionMaxHours
           && declinationDeg >= kDeclinationMinDeg && declinationDeg <= kDeclinationMaxDeg;
}

}  // namespace skygate::core
