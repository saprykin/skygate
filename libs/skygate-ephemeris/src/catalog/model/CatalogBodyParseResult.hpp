#pragma once

#include "catalog/CatalogLoadResult.hpp"
#include "Types.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct CatalogBodyParseResult {
    std::vector<CelestialBody> bodies;
    CatalogLoadErrorCode errorCode = CatalogLoadErrorCode::NoError;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return errorCode == CatalogLoadErrorCode::NoError;
    }
};

}  // namespace skygate::ephemeris
