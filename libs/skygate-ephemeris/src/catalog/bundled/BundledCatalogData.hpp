#pragma once

#include "BaseCelestialBody.hpp"
#include "DeepSkyObjectInfo.hpp"

#include <span>
#include <string_view>

namespace skygate::ephemeris {

// Immutable table of the bodies bundled with the application.
//
// The records follow the model they populate: own-galaxy bodies in the first
// table and deep-sky objects in the second. The arrays stay in the
// implementation, so consumers only ever hold spans over constant storage.
class BundledCatalogData final {
public:
    struct Entry final {
        std::string_view id;
        std::string_view displayName;
        BaseCelestialBody::Kind type;
        double visualMagnitude;
    };

    struct DeepSkyObjectEntry final {
        std::string_view id;
        std::string_view displayName;
        DeepSkyObjectInfo::Kind kind;
        double rightAscensionHours;
        double declinationDeg;
        double visualMagnitude;
        double majorAxisArcmin;
        double minorAxisArcmin;
        double positionAngleDeg;
        std::string_view aliases;
    };

    [[nodiscard]] static std::span<const Entry> entries() noexcept;
    [[nodiscard]] static std::span<const DeepSkyObjectEntry> messierEntries() noexcept;
};

}  // namespace skygate::ephemeris
