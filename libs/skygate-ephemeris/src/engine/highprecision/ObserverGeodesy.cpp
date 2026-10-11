#include "ObserverGeodesy.hpp"

#include "math/PhysicalConstants.hpp"
#include "math/Wgs84Geodesy.hpp"

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::PhysicalConstants;

}  // namespace

std::optional<skygate::core::Vector3d>
ObserverGeodesy::observerItrsPositionAu(const skygate::core::GeoLocation& observer) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }

    const skygate::core::Vector3d positionMeters = skygate::core::Wgs84Geodesy::ecefMeters(observer);
    return skygate::core::Vector3d{
        .x = positionMeters.x / PhysicalConstants::kAstronomicalUnitMeters,
        .y = positionMeters.y / PhysicalConstants::kAstronomicalUnitMeters,
        .z = positionMeters.z / PhysicalConstants::kAstronomicalUnitMeters,
    };
}

}  // namespace skygate::ephemeris::highprecision
