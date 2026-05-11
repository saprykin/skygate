#pragma once

#include "skygate/ephemeris/CatalogLoadResult.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct CatalogBodyParseResult {
    std::vector<CelestialBody> bodies;
    CatalogLoadErrorCode errorCode = CatalogLoadErrorCode::None;
    std::string errorDetail;
    CatalogLoadDiagnostics diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return errorCode == CatalogLoadErrorCode::None;
    }
};

}  // namespace skygate::ephemeris
