#pragma once

#include "catalog/CatalogSourceType.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogPayloadFormatDetector final {
public:
    [[nodiscard]] static CatalogSourceType detect(std::string_view payload) noexcept;
};

}  // namespace skygate::ephemeris
