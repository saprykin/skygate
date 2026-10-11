#pragma once

#include "time/AstronomicalEpoch.hpp"
#include "time/AstronomicalEpochRange.hpp"

#include <string>

namespace skygate::ephemeris {

struct EphemerisDateRange {
    std::string id;
    std::string displayName;
    skygate::core::AstronomicalEpoch start;
    skygate::core::AstronomicalEpoch end;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool contains(const skygate::core::AstronomicalEpoch& epoch) const noexcept;
    [[nodiscard]] skygate::core::AstronomicalEpochRange interval() const noexcept;

    friend bool operator==(const EphemerisDateRange&, const EphemerisDateRange&) = default;
};

}  // namespace skygate::ephemeris
