#include "skygate/core/EquatorialCoordinate.hpp"

#include <cmath>

namespace skygate::core {

bool EquatorialCoordinate::isFinite() const noexcept
{
    return std::isfinite(rightAscensionHours) && std::isfinite(declinationDeg);
}

}  // namespace skygate::core
