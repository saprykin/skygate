#pragma once

#include "catalog/CatalogLoadResult.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogPayloadFormatDetector final {
public:
    [[nodiscard]] static CatalogPayloadFormat detect(std::string_view payload) noexcept;
};

}  // namespace skygate::ephemeris
