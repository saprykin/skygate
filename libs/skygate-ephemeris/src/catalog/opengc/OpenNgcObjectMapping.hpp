#pragma once

#include "DeepSkyObjectInfo.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct OpenNgcObjectMapping final {
    std::string id;
    std::string displayName;
    DeepSkyObjectInfo::Kind kind = DeepSkyObjectInfo::Kind::Unknown;
    std::vector<std::string> aliases;
};

}  // namespace skygate::ephemeris
