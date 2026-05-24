#pragma once

#include "Types.hpp"

namespace skygate::ephemeris {

enum class EphemerisPrecisionPolicy : std::uint8_t {
    SceneRender,
    SelectionDetail,
    Trail,
    EventSearch,
    NightConditionsApproximate,
    NightConditionsVerified
};

// Full-scene rendering and selected-object trails intentionally use a lean
// topocentric request in high-precision mode so broad redraws stay interactive.
// Selection details, event search, and verified night-condition calculations
// keep the caller's complete correction set.
[[nodiscard]] EphemerisRequest
ephemerisRequestForPrecisionPolicy(EphemerisRequest request, EphemerisPrecisionPolicy policy) noexcept;

}  // namespace skygate::ephemeris
