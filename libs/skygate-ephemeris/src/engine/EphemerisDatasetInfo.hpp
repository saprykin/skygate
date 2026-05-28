#pragma once

#include "EphemerisDateRange.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisDatasetInfo {
    std::string id;
    std::string displayName;
    std::string version;
    std::string provenance;
    std::vector<EphemerisDateRange> dateRanges;
};

}  // namespace skygate::ephemeris
