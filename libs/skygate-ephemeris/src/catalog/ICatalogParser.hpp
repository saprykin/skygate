#pragma once

#include "CatalogBodyParseResult.hpp"
#include "CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

class ICatalogParser {
public:
    virtual ~ICatalogParser() = default;

    [[nodiscard]] virtual CatalogBodyParseResult
    parse(std::string_view data, const CatalogParseProgressCallback& progressCallback) const = 0;
};

}  // namespace skygate::ephemeris
