#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class CatalogSourceType : std::uint8_t {
    Bundled,
    HygCsv,
    OpenNgcCsv,
    Unknown
};

}  // namespace skygate::ephemeris
