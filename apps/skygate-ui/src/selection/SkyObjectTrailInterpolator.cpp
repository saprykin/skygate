#include "SkyObjectTrailInterpolator.hpp"
#include "CelestialBodyCatalog.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "math/SphericalGeometry.hpp"
#include "trail/BodyTrailCalculator.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace {

constexpr int kAdaptiveSampleStepMinutes = 120;
// Refinement stops at a fixed span; the render grid step may differ.
constexpr int kObjectTrailAdaptiveRefinementSpanMinutes = 10;
constexpr double kMaxInterpolationErrorDegrees = 0.05;

}  // namespace

std::optional<skygate::core::HorizontalCoordinate> SkyObjectTrailInterpolator::interpolateSampleAtOffset(
    const std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const int offsetMinutes,
    std::size_t& nextAnchorIndex
)
{
    while (nextAnchorIndex < anchors.size() && anchors[nextAnchorIndex].offsetMinutes < offsetMinutes) {
        ++nextAnchorIndex;
    }
    if (nextAnchorIndex >= anchors.size()) {
        return std::nullopt;
    }

    const skygate::ephemeris::BodyTrailSample& next = anchors[nextAnchorIndex];
    if (next.offsetMinutes == offsetMinutes) {
        return next.horizontal;
    }
    if (nextAnchorIndex == 0U) {
        return std::nullopt;
    }

    const skygate::ephemeris::BodyTrailSample& previous = anchors[nextAnchorIndex - 1U];
    if (!previous.horizontal.has_value() || !next.horizontal.has_value()) {
        return std::nullopt;
    }

    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= 0) {
        return std::nullopt;
    }

    return skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(
        *previous.horizontal,
        *next.horizontal,
        static_cast<double>(offsetMinutes - previous.offsetMinutes) / static_cast<double>(spanMinutes)
    );
}

std::vector<skygate::ephemeris::BodyTrailSample> SkyObjectTrailInterpolator::interpolateAnchors(
    const std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    std::vector<skygate::ephemeris::BodyTrailSample> samples =
        skygate::ephemeris::BodyTrailCalculator::sampleOffsets(renderOptions);

    std::size_t nextAnchorIndex = 0U;
    for (skygate::ephemeris::BodyTrailSample& sample : samples) {
        sample.horizontal = interpolateSampleAtOffset(anchors, sample.offsetMinutes, nextAnchorIndex);
    }

    return samples;
}

skygate::ephemeris::BodyTrailSample
SkyObjectTrailInterpolator::sampleEngineAtOffset(const SkyObjectTrailSamplingInput& input, const int offsetMinutes)
{
    if (!input.ephemerisRequest.has_value()) {
        return skygate::ephemeris::BodyTrailSample{.offsetMinutes = offsetMinutes, .horizontal = std::nullopt};
    }

    return skygate::ephemeris::BodyTrailSample{
        .offsetMinutes = offsetMinutes,
        .horizontal = skygate::ephemeris::BodyTrailCalculator::sampleAtOffset(
            *input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, offsetMinutes
        )
    };
}

std::vector<skygate::ephemeris::BodyTrailSample> SkyObjectTrailInterpolator::sampleEngineWindow(
    const SkyObjectTrailSamplingInput& input, const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const skygate::ephemeris::BodyTrailCalculator trailCalculator;
    if (input.ephemerisRequest.has_value()) {
        return trailCalculator.sample(
            *input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, renderOptions
        );
    }

    return trailCalculator.sample(*input.ephemerisEngine, input.skyContext, input.targetBodyIndex, renderOptions);
}

bool SkyObjectTrailInterpolator::interpolationErrorWithinBounds(
    const skygate::ephemeris::BodyTrailSample& previous,
    const skygate::ephemeris::BodyTrailSample& midpoint,
    const skygate::ephemeris::BodyTrailSample& next
) noexcept
{
    if (!previous.horizontal.has_value() || !midpoint.horizontal.has_value() || !next.horizontal.has_value()) {
        return true;
    }

    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= 0) {
        return true;
    }

    const skygate::core::HorizontalCoordinate interpolated =
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(
            *previous.horizontal,
            *next.horizontal,
            static_cast<double>(midpoint.offsetMinutes - previous.offsetMinutes) / static_cast<double>(spanMinutes)
        );
    return skygate::core::SphericalGeometry::angularSeparationDegrees(interpolated, *midpoint.horizontal)
           <= kMaxInterpolationErrorDegrees;
}

void SkyObjectTrailInterpolator::appendAdaptiveTrailAnchors(
    std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const SkyObjectTrailSamplingInput& input,
    const skygate::ephemeris::BodyTrailSample& previous,
    const skygate::ephemeris::BodyTrailSample& next
)
{
    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= kObjectTrailAdaptiveRefinementSpanMinutes) {
        anchors.push_back(next);
        return;
    }

    const int midpointOffsetMinutes = previous.offsetMinutes + (spanMinutes / 2);
    if (midpointOffsetMinutes <= previous.offsetMinutes || midpointOffsetMinutes >= next.offsetMinutes) {
        anchors.push_back(next);
        return;
    }

    const skygate::ephemeris::BodyTrailSample midpoint = sampleEngineAtOffset(input, midpointOffsetMinutes);
    if (interpolationErrorWithinBounds(previous, midpoint, next)) {
        anchors.push_back(next);
        return;
    }

    appendAdaptiveTrailAnchors(anchors, input, previous, midpoint);
    appendAdaptiveTrailAnchors(anchors, input, midpoint, next);
}

std::vector<skygate::ephemeris::BodyTrailSample> SkyObjectTrailInterpolator::sampleAdaptiveTrail(
    const SkyObjectTrailSamplingInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const skygate::ephemeris::BodyTrailOptions anchorOptions{
        .pastHours = renderOptions.pastHours,
        .futureHours = renderOptions.futureHours,
        .sampleStepMinutes = kAdaptiveSampleStepMinutes
    };
    const std::vector<skygate::ephemeris::BodyTrailSample> coarseAnchors =
        trailCalculator.sample(*input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, anchorOptions);
    if (coarseAnchors.size() < 2U) {
        return interpolateAnchors(coarseAnchors, renderOptions);
    }

    std::vector<skygate::ephemeris::BodyTrailSample> adaptiveAnchors;
    adaptiveAnchors.reserve(coarseAnchors.size());
    adaptiveAnchors.push_back(coarseAnchors.front());
    for (std::size_t index = 1U; index < coarseAnchors.size(); ++index) {
        appendAdaptiveTrailAnchors(adaptiveAnchors, input, coarseAnchors[index - 1U], coarseAnchors[index]);
    }

    return interpolateAnchors(adaptiveAnchors, renderOptions);
}

std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> SkyObjectTrailInterpolator::sampleGuidanceTrail(
    const SkyObjectTrailSamplingInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    if (input.guidanceStrategy == nullptr) {
        return std::nullopt;
    }

    // The guidance engine resolves the target through a catalog, so the trail
    // passes the selected body with all of its catalog metadata.
    skygate::ephemeris::CelestialBodyCatalog catalog;
    catalog.appendBody(*input.targetBody);

    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> guidanceEngine =
        input.guidanceStrategy->createGuidanceEngine(catalog);
    if (guidanceEngine == nullptr) {
        return std::nullopt;
    }

    skygate::ephemeris::EphemerisRequest guidanceRequest = *input.ephemerisRequest;
    guidanceRequest.options = guidanceEngine->options();
    guidanceRequest.options.setEngineKind(guidanceEngine->kind());
    std::vector<skygate::ephemeris::BodyTrailSample> samples =
        trailCalculator.sample(*guidanceEngine, guidanceRequest, 0U, renderOptions);
    return samples;
}
