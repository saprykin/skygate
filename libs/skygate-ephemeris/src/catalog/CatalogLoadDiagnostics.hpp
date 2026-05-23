#pragma once

#include <cstddef>

namespace skygate::ephemeris {

struct CatalogLoadDiagnostics {
    std::size_t processedRowCount = 0;
    std::size_t parsedBodyCount = 0;
    std::size_t selectedBodyCount = 0;
    std::size_t truncatedBodyCount = 0;
};

}  // namespace skygate::ephemeris
