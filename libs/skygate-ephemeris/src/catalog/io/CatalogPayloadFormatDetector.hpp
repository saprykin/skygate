#pragma once

#include "catalog/CatalogSourceType.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogPayloadFormatDetector final {
public:
    // Detects the catalog schema of an already-decoded (plain-text) payload.
    // Archive containers are decoded by the payload parser before this runs.
    [[nodiscard]] static CatalogSourceType detect(std::string_view payload) noexcept;
};

}  // namespace skygate::ephemeris
