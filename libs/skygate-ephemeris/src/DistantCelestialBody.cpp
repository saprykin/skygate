#include "DistantCelestialBody.hpp"

namespace skygate::ephemeris {

const std::optional<DeepSkyObjectInfo>& DistantCelestialBody::deepSkyObjectValue() const noexcept
{
    return deepSkyObject;
}

}  // namespace skygate::ephemeris
