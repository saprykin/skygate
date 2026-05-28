#pragma once

#include "CatalogLoadDiagnostics.hpp"
#include "CatalogLoadResult.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct CatalogBodyParseResult {
    std::vector<OwnGalaxyCelestialBody> bodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes;
    CatalogLoadResult::ErrorCode errorCode = CatalogLoadResult::ErrorCode::NoError;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
