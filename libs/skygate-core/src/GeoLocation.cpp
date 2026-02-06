#include "skygate/core/Types.hpp"

#include <cmath>

namespace skygate::core {

bool GeoLocation::isValid() const noexcept
{
    return std::isfinite(latitudeDeg) && std::isfinite(longitudeDeg)
        && std::isfinite(elevationMeters)
        && latitudeDeg >= -90.0 && latitudeDeg <= 90.0
        && longitudeDeg >= -180.0 && longitudeDeg <= 180.0;
}

}  // namespace skygate::core
