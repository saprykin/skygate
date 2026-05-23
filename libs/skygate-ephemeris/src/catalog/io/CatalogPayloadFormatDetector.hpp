#pragma once

#include "catalog/CatalogLoadResult.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogPayloadFormatDetector final {
public:
    [[nodiscard]] static CatalogLoadResult::PayloadFormat detect(std::string_view payload) noexcept;
};

}  // namespace skygate::ephemeris
