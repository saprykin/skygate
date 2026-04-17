#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace skygate::ephemeris {

enum class CatalogPayloadFormat : std::uint8_t {
    PipeRows,
    HygCsv,
    HygCsvGzip,
    HygCsvZip,
    Unknown
};

class CatalogPayloadParser final {
public:
    [[nodiscard]] CatalogPayloadFormat detectFormat(std::string_view payload) const noexcept;

    [[nodiscard]] std::unique_ptr<IStarCatalog> parse(
        std::string_view payload,
        const HygParseProgressCallback& progressCallback = {}
    ) const;
};

}  // namespace skygate::ephemeris
