#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct OpenNgcObjectMapping final {
    std::string id;
    std::string displayName;
    DeepSkyObjectKind kind = DeepSkyObjectKind::Unknown;
    std::vector<std::string> aliases;
};

}  // namespace skygate::ephemeris
