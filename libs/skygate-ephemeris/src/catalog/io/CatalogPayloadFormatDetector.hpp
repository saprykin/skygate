#pragma once

#include "skygate/ephemeris/CatalogLoadResult.hpp"

#include <string_view>

namespace skygate::ephemeris {

class CatalogPayloadFormatDetector final {
public:
    [[nodiscard]] static CatalogPayloadFormat detect(std::string_view payload) noexcept;
};

}  // namespace skygate::ephemeris
