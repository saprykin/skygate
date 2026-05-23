#pragma once

#include "Types.hpp"

#include <QString>

#include <optional>

namespace skygate::ephemeris {

struct RowParseOutcome {
    std::optional<CelestialBody> body;
    int invalidCategoryIndex = -1;
    QString invalidSample;
};

}  // namespace skygate::ephemeris
