#pragma once

#include "SkyObjectTrailBuilder.hpp"

#include <optional>

namespace skygate::ui::tests {

// Default trail-builder inputs shared by the trail geometry and guidance
// suites: an observer at the fixed test location with the standard viewport.
class TrailBuilderTestScenario final {
public:
    [[nodiscard]] static std::optional<skygate::core::PreparedProjection> makeProjection(double fovDeg = 90.0);
    [[nodiscard]] static SkyObjectTrailInput
    makeInput(const skygate::ephemeris::IEphemerisEngine& engine, const skygate::core::PreparedProjection& projection);
};

}  // namespace skygate::ui::tests
