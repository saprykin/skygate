#include "GeoLocation.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

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

bool GeoLocation::isIdenticalTo(const GeoLocation& other) const noexcept
{
    return std::bit_cast<std::uint64_t>(latitudeDeg) == std::bit_cast<std::uint64_t>(other.latitudeDeg)
           && std::bit_cast<std::uint64_t>(longitudeDeg) == std::bit_cast<std::uint64_t>(other.longitudeDeg)
           && std::bit_cast<std::uint64_t>(elevationMeters) == std::bit_cast<std::uint64_t>(other.elevationMeters);
}

}  // namespace skygate::core
