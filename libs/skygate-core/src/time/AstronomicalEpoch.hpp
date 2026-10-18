#pragma once

#include "TimeScale.hpp"

#include <optional>

namespace skygate::core {

struct AstronomicalEpoch {
    double julianDatePart1 = 0.0;
    double julianDatePart2 = 0.0;
    TimeScale timeScale = TimeScale::Utc;

    [[nodiscard]] AstronomicalEpoch normalized() const noexcept;

    [[nodiscard]] bool hasExplicit() const noexcept;
    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isFiniteUtc() const noexcept;
    [[nodiscard]] double sortKey() const noexcept;

    // Bit identity for cache keys: +0.0 and -0.0 differ here even though
    // operator== treats them as equal.
    [[nodiscard]] bool isIdenticalTo(const AstronomicalEpoch& other) const noexcept;

    [[nodiscard]] AstronomicalEpoch addSeconds(double offsetSeconds) const noexcept;
    [[nodiscard]] AstronomicalEpoch addSeconds(double offsetSeconds, TimeScale resultScale) const noexcept;
    [[nodiscard]] AstronomicalEpoch addMinutes(double offsetMinutes) const noexcept;
    [[nodiscard]] AstronomicalEpoch addMinutes(double offsetMinutes, TimeScale resultScale) const noexcept;
    [[nodiscard]] AstronomicalEpoch addDays(double offsetDays) const noexcept;
    [[nodiscard]] AstronomicalEpoch addDays(double offsetDays, TimeScale resultScale) const noexcept;

    friend bool operator==(const AstronomicalEpoch&, const AstronomicalEpoch&) = default;
};

using AstronomicalEpochResult = std::optional<AstronomicalEpoch>;

}  // namespace skygate::core
