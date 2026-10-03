#include "SimpleBodyStateCalculator.hpp"
#include "BaseCelestialBody.hpp"

#include <optional>

namespace skygate::ephemeris {

std::optional<skygate::core::EquatorialCoordinate> SimpleBodyStateCalculator::computeEquatorial(
    const BaseCelestialBody& body, const skygate::core::UtcTimePoint& utcTime
) const noexcept
{
    if (body.fixedEquatorialValue().has_value()) {
        return body.fixedEquatorialValue();
    }

    switch (body.kind) {
    case BaseCelestialBody::Kind::Sun:
        return m_sunCalculator.compute(utcTime);
    case BaseCelestialBody::Kind::Moon:
        return m_moonCalculator.compute(utcTime);
    case BaseCelestialBody::Kind::Planet:
        return m_planetCalculator.compute(body.id, utcTime);
    case BaseCelestialBody::Kind::Star:
    case BaseCelestialBody::Kind::Constellation:
    case BaseCelestialBody::Kind::DeepSkyObject:
        break;
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
