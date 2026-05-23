#pragma once

#include "catalog/CatalogSelectionOptions.hpp"
#include "catalog/CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

struct CatalogParseRequest {
    std::string_view payload;
    HygParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
};

}  // namespace skygate::ephemeris
