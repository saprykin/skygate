#include "GeoLocation.hpp"

#include <cmath>

namespace skygate::core {

bool GeoLocation::isValid() const noexcept
{
    return std::isfinite(latitudeDeg) && std::isfinite(longitudeDeg) && std::isfinite(elevationMeters)
           && latitudeDeg >= kLatitudeMinDeg && latitudeDeg <= kLatitudeMaxDeg && longitudeDeg >= kLongitudeMinDeg
           && longitudeDeg <= kLongitudeMaxDeg;
}

bool GeoLocation::equals(const GeoLocation& other) const noexcept
{
    return latitudeDeg == other.latitudeDeg && longitudeDeg == other.longitudeDeg
           && elevationMeters == other.elevationMeters;
}

}  // namespace skygate::core
