#pragma once

#include "catalog/CatalogLoadResult.hpp"

#include <cstddef>
#include <string>

namespace skygate::ephemeris {

struct DelimitedCatalogParserOptions {
    std::size_t progressInterval = 512;
    std::size_t maxInvalidRowSamples = 5;
    CatalogLoadResult::ErrorCode zeroResultCode = CatalogLoadResult::ErrorCode::NoBodies;
    std::string zeroResultDetail;
    std::string formatName;
};

}  // namespace skygate::ephemeris
