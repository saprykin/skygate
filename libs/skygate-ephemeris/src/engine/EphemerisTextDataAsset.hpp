#pragma once

#include <string>

namespace skygate::ephemeris {

struct EphemerisTextDataAsset {
    std::string id;
    std::string version;
    std::string provenance;
    std::string content;
};

}  // namespace skygate::ephemeris
