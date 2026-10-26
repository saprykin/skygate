#pragma once

#include "CatalogSourceType.hpp"

#include <cstddef>
#include <functional>
#include <string_view>

namespace skygate::ephemeris {

using CatalogParseProgressCallback = std::function<void(std::size_t parsedObjectCount)>;

struct CatalogSourceRequest {
    CatalogSourceType type = CatalogSourceType::Bundled;
    std::string_view data;
    CatalogParseProgressCallback progressCallback;
};

}  // namespace skygate::ephemeris
