#pragma once

#include "time/TimeScale.hpp"

#include <cstdint>
#include <optional>

namespace skygate::ephemeris {

struct CivilDateTime {
    int astronomicalYear = 2000;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    std::uint32_t nanosecond = 0U;
    TimeScale timeScale = TimeScale::Utc;
};

using CivilDateTimeResult = std::optional<CivilDateTime>;

}  // namespace skygate::ephemeris
