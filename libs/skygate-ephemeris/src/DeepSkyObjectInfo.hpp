#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

    [[nodiscard]] static constexpr std::string_view kindLabel(const Kind kind) noexcept
    {
        switch (kind) {
        case Kind::Unknown:
            return "Deep sky object";
        case Kind::Galaxy:
            return "Galaxy";
        case Kind::OpenCluster:
            return "Open cluster";
        case Kind::GlobularCluster:
            return "Globular cluster";
        case Kind::Nebula:
            return "Nebula";
        case Kind::PlanetaryNebula:
            return "Planetary nebula";
        case Kind::Asterism:
            return "Asterism";
        }

        return "Deep sky object";
    }

    Kind kind = Kind::Unknown;
    std::vector<std::string> aliases;
    std::optional<double> majorAxisArcmin;
    std::optional<double> minorAxisArcmin;
    std::optional<double> positionAngleDeg;
};

}  // namespace skygate::ephemeris
