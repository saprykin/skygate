#pragma once

#include "catalog/CatalogBodyParseResult.hpp"
#include "catalog/CatalogSourceRequest.hpp"

#include <functional>
#include <string_view>

namespace skygate::ephemeris {

class CompressedCatalogParser final {
public:
    using InnerParser =
        std::function<CatalogBodyParseResult(std::string_view data, const HygParseProgressCallback& progressCallback)>;

    [[nodiscard]] static CatalogBodyParseResult parse(
        std::string_view compressedData,
        const HygParseProgressCallback& progressCallback,
        const InnerParser& innerParser,
        const std::string& emptyDetail = "Gzip catalog payload is empty.",
        const std::string& decompressDetail = "Gzip catalog payload could not be decompressed."
    );
};

}  // namespace skygate::ephemeris
