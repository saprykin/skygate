#pragma once

#include "HorizontalCoordinate.hpp"
#include "SkyObjectTrailSamplingInput.hpp"
#include "trail/BodyTrailOptions.hpp"
#include "trail/BodyTrailSample.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace skygate::ephemeris {
class BodyTrailCalculator;
}  // namespace skygate::ephemeris

// Turns engine states into trail samples on the render grid: direct engine
// sampling, guidance-engine sampling, adaptive anchor refinement, and the
// interpolation that fills the grid between anchors.
class SkyObjectTrailInterpolator final {
public:
    SkyObjectTrailInterpolator() = delete;

    // One engine sample per render step across the whole window.
    [[nodiscard]] static std::vector<skygate::ephemeris::BodyTrailSample> sampleEngineWindow(
        const SkyObjectTrailSamplingInput& input, const skygate::ephemeris::BodyTrailOptions& renderOptions
    );

    // Anchors refined until the linear interpolation error stays inside the
    // trail error bound, then resampled on the render grid.
    [[nodiscard]] static std::vector<skygate::ephemeris::BodyTrailSample> sampleAdaptiveTrail(
        const SkyObjectTrailSamplingInput& input,
        const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
        const skygate::ephemeris::BodyTrailOptions& renderOptions
    );

    // Nullopt when there is no guidance strategy or it cannot build an engine.
    [[nodiscard]] static std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> sampleGuidanceTrail(
        const SkyObjectTrailSamplingInput& input,
        const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
        const skygate::ephemeris::BodyTrailOptions& renderOptions
    );

    // One sample per render step, interpolated from `anchors`.
    [[nodiscard]] static std::vector<skygate::ephemeris::BodyTrailSample> interpolateAnchors(
        const std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
        const skygate::ephemeris::BodyTrailOptions& renderOptions
    );

private:
    [[nodiscard]] static skygate::ephemeris::BodyTrailSample
    sampleEngineAtOffset(const SkyObjectTrailSamplingInput& input, int offsetMinutes);

    [[nodiscard]] static std::optional<skygate::core::HorizontalCoordinate> interpolateSampleAtOffset(
        const std::vector<skygate::ephemeris::BodyTrailSample>& anchors, int offsetMinutes, std::size_t& nextAnchorIndex
    );

    [[nodiscard]] static bool interpolationErrorWithinBounds(
        const skygate::ephemeris::BodyTrailSample& previous,
        const skygate::ephemeris::BodyTrailSample& midpoint,
        const skygate::ephemeris::BodyTrailSample& next
    ) noexcept;

    static void appendAdaptiveTrailAnchors(
        std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
        const SkyObjectTrailSamplingInput& input,
        const skygate::ephemeris::BodyTrailSample& previous,
        const skygate::ephemeris::BodyTrailSample& next
    );
};
