#pragma once

#include "time/TimeScale.hpp"

#include <optional>

namespace skygate::ephemeris {

struct AstronomicalEpoch {
    double julianDatePart1 = 0.0;
    double julianDatePart2 = 0.0;
    TimeScale timeScale = TimeScale::Utc;
};

using AstronomicalEpochResult = std::optional<AstronomicalEpoch>;

[[nodiscard]] AstronomicalEpoch normalizedAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept;

[[nodiscard]] bool hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept;
[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept;
[[nodiscard]] bool isFiniteUtcEpoch(const AstronomicalEpoch& epoch) noexcept;
[[nodiscard]] double epochSortKey(const AstronomicalEpoch& epoch) noexcept;

}  // namespace skygate::ephemeris
