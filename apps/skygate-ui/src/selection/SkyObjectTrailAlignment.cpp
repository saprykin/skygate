#include "SkyObjectTrailAlignment.hpp"
#include "CelestialBodyState.hpp"
#include "math/SphericalGeometry.hpp"
#include "math/Vector3d.hpp"

#include <algorithm>

void SkyObjectTrailAlignment::alignToSelectedState(
    std::vector<skygate::ephemeris::BodyTrailSample>& samples, const SkyObjectTrailSamplingInput& input
)
{
    if (input.targetState == nullptr || !input.targetState->horizontal.isFinite()) {
        return;
    }

    auto presentSample =
        std::find_if(samples.begin(), samples.end(), [](const skygate::ephemeris::BodyTrailSample& sample) {
            return sample.offsetMinutes == 0 && sample.horizontal.has_value() && sample.horizontal->isFinite();
        });
    if (presentSample == samples.end()) {
        return;
    }

    const skygate::core::HorizontalCoordinate selectedHorizontal = input.targetState->horizontal;
    const skygate::core::Vector3d guidanceNow =
        skygate::core::SphericalGeometry::horizontalToUnitVector(*presentSample->horizontal);
    const skygate::core::Vector3d selectedNow =
        skygate::core::SphericalGeometry::horizontalToUnitVector(selectedHorizontal);
    const skygate::core::Vector3d axis =
        skygate::core::SphericalGeometry::rotationAxisBetween(guidanceNow, selectedNow);
    const double angleRad = skygate::core::SphericalGeometry::rotationAngleBetween(guidanceNow, selectedNow, axis);
    if (angleRad <= 0.0) {
        // Nothing to rotate, so the sampled directions keep their exact values.
        presentSample->horizontal = selectedHorizontal;
        return;
    }

    for (skygate::ephemeris::BodyTrailSample& sample : samples) {
        if (sample.horizontal.has_value() && sample.horizontal->isFinite()) {
            sample.horizontal = skygate::core::SphericalGeometry::horizontalFromUnitVector(
                skygate::core::SphericalGeometry::rotatedAroundAxis(
                    skygate::core::SphericalGeometry::horizontalToUnitVector(*sample.horizontal), axis, angleRad
                )
            );
        }
    }
    presentSample->horizontal = selectedHorizontal;
}
