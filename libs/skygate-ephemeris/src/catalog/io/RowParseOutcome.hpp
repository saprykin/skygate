#pragma once

#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <QString>

#include <optional>

namespace skygate::ephemeris {

struct RowParseOutcome {
    std::optional<OwnGalaxyCelestialBody> body;
    std::optional<DistantCelestialBody> distantBody;
    int invalidCategoryIndex = -1;
    QString invalidSample;
};

}  // namespace skygate::ephemeris
