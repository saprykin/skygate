#pragma once

#include "catalog/CatalogLoadDiagnostics.hpp"
#include "catalog/CatalogLoadResult.hpp"
#include "Types.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct CatalogBodyParseResult {
    std::vector<CelestialBody> bodies;
    CatalogLoadResult::ErrorCode errorCode = CatalogLoadResult::ErrorCode::NoError;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return errorCode == CatalogLoadResult::ErrorCode::NoError;
    }
};

}  // namespace skygate::ephemeris
