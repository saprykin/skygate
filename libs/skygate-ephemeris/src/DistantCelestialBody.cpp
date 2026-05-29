#include "DistantCelestialBody.hpp"

namespace skygate::ephemeris {

const std::optional<skygate::core::EquatorialCoordinate>& DistantCelestialBody::fixedEquatorialValue() const noexcept
{
    return fixedEquatorial;
}

const std::optional<DeepSkyObjectInfo>& DistantCelestialBody::deepSkyObjectValue() const noexcept
{
    return deepSkyObject;
}

}  // namespace skygate::ephemeris
