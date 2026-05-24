#pragma once

#include "HorizontalCoordinate.hpp"

#include <optional>

namespace skygate::ephemeris {

struct BodyTrailSample final {
    int offsetMinutes = 0;
    std::optional<skygate::core::HorizontalCoordinate> horizontal;
};

}  // namespace skygate::ephemeris
