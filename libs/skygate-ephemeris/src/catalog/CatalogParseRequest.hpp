#pragma once

#include "CatalogSelectionOptions.hpp"
#include "CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

struct CatalogParseRequest {
    std::string_view payload;
    CatalogParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
};

}  // namespace skygate::ephemeris
