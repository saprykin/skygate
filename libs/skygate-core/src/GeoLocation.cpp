#include "skygate/core/GeoLocation.hpp"

#include <cmath>

namespace skygate::core {

bool GeoLocation::isValid() const noexcept
{
    return std::isfinite(latitudeDeg) && std::isfinite(longitudeDeg) && std::isfinite(elevationMeters)
           && latitudeDeg >= kLatitudeMinDeg && latitudeDeg <= kLatitudeMaxDeg && longitudeDeg >= kLongitudeMinDeg
           && longitudeDeg <= kLongitudeMaxDeg;
}

}  // namespace skygate::core
