#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class CatalogSourceType : std::uint8_t {
    Bundled,
    HygCsv,
    HygCsvGzip,
    OpenNgcCsv
};

}  // namespace skygate::ephemeris
