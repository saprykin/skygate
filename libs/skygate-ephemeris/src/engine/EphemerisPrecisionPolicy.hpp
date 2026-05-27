#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisPrecisionPolicy : std::uint8_t {
    SceneRender,
    SelectionDetail,
    Trail,
    EventSearch,
    NightConditionsApproximate,
    NightConditionsVerified
};

}  // namespace skygate::ephemeris
