#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace skygate::ephemeris {

struct DeepSkyObjectInfo {
    enum class Kind : std::uint8_t {
        Unknown,
        Galaxy,
        OpenCluster,
        GlobularCluster,
        Nebula,
        PlanetaryNebula,
        Asterism
    };

    Kind kind = Kind::Unknown;
    std::vector<std::string> aliases;
    std::optional<double> majorAxisArcmin;
    std::optional<double> minorAxisArcmin;
    std::optional<double> positionAngleDeg;
};

}  // namespace skygate::ephemeris
