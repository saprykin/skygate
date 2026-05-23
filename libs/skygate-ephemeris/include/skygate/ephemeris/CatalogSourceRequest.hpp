#pragma once

#include "skygate/ephemeris/CatalogSelectionOptions.hpp"
#include "skygate/ephemeris/CatalogSourceType.hpp"

#include <cstddef>
#include <functional>
#include <string_view>

namespace skygate::ephemeris {

using HygParseProgressCallback = std::function<void(std::size_t parsedObjectCount)>;

struct CatalogSourceRequest {
    CatalogSourceType type = CatalogSourceType::Bundled;
    std::string_view data;
    HygParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
};

}  // namespace skygate::ephemeris
