#pragma once

#include "TimeScale.hpp"

#include <optional>

namespace skygate::ephemeris {

struct AstronomicalEpoch {
    double julianDatePart1 = 0.0;
    double julianDatePart2 = 0.0;
    TimeScale timeScale = TimeScale::Utc;

    [[nodiscard]] AstronomicalEpoch normalized() const noexcept;

    [[nodiscard]] bool hasExplicit() const noexcept;
    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isFiniteUtc() const noexcept;
    [[nodiscard]] double sortKey() const noexcept;

    [[nodiscard]] AstronomicalEpoch addSeconds(double offsetSeconds) const noexcept;
    [[nodiscard]] AstronomicalEpoch addSeconds(double offsetSeconds, TimeScale resultScale) const noexcept;
    [[nodiscard]] AstronomicalEpoch addMinutes(double offsetMinutes) const noexcept;
    [[nodiscard]] AstronomicalEpoch addMinutes(double offsetMinutes, TimeScale resultScale) const noexcept;
};

using AstronomicalEpochResult = std::optional<AstronomicalEpoch>;

}  // namespace skygate::ephemeris
