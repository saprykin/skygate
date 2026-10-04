#pragma once

#include "CatalogSelectionOptions.hpp"
#include "CatalogSourceRequest.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

struct CatalogParseRequest {
    std::string_view payload;
    CatalogParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
    // Optional archive member name. When present, the named ZIP member is
    // selected exactly and must exist; otherwise the unique supported member is
    // chosen deterministically and ambiguity is reported.
    std::optional<std::string> memberSelector;
};

}  // namespace skygate::ephemeris
