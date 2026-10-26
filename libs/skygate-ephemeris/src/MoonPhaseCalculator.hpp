#pragma once

#include "MoonPhase.hpp"
#include "time/AstronomicalEpoch.hpp"

namespace skygate::ephemeris {

class MoonPhaseCalculator final {
public:
    [[nodiscard]] MoonPhase compute(const skygate::core::AstronomicalEpoch& epoch) const noexcept;
};

}  // namespace skygate::ephemeris
