#pragma once

#include <string>

namespace skygate::ephemeris {

struct EphemerisKernelDataAsset {
    std::string id;
    std::string profileId;
    std::string version;
    std::string provenance;
    std::string activePath;
};

}  // namespace skygate::ephemeris
