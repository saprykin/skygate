#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/CivilDateTime.hpp"
#include "time/TimeScale.hpp"

#include <cassert>
#include <cmath>
#include <span>
#include <string_view>

namespace skygate::ephemeris::tests {

[[nodiscard]] inline bool isNear(const double value, const double expected, const double tolerance) noexcept
{
    return std::abs(value - expected) <= tolerance;
}

[[nodiscard]] inline const BaseCelestialBody*
findBodyById(const std::span<const BaseCelestialBody* const> bodies, const std::string_view id)
{
    for (const BaseCelestialBody* body : bodies) {
        if (body != nullptr && body->id == id) {
            return body;
        }
    }
    return nullptr;
}

[[nodiscard]] inline const CelestialBodyState*
findStateById(const EphemerisSnapshot& snapshot, const std::string_view id)
{
    for (const CelestialBodyState& state : snapshot.states) {
        if (snapshot.bodyAt(state.bodyIndex).id == id) {
            return &state;
        }
    }
    return nullptr;
}

[[nodiscard]] inline skygate::core::AstronomicalEpoch dateEpoch(const int year, const int month, const int day)
{
    const auto epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    assert(epoch.has_value());
    return *epoch;
}

[[nodiscard]] inline skygate::core::CivilDateTime dateFromEpoch(const skygate::core::AstronomicalEpoch& epoch)
{
    const auto date = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch);
    assert(date.has_value());
    return *date;
}

}  // namespace skygate::ephemeris::tests
