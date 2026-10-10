#pragma once

#include "BaseCelestialBody.hpp"
#include "EphemerisRequest.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "math/Vector3d.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "engine/highprecision/CelestialFrameTransformResult.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"

#include <string_view>

namespace skygate::ephemeris::tests {

// Builds requests, catalog bodies, calculator inputs and results, and
// frame-transform results shared by the apparent-place test suites.
class ApparentPlaceTestFixture final {
public:
    [[nodiscard]] static EphemerisRequest request(EphemerisCorrectionFlags correctionFlags);

    [[nodiscard]] static OwnGalaxyCelestialBody sunBody();
    [[nodiscard]] static OwnGalaxyCelestialBody body(std::string_view id, std::string_view bodyType);

    [[nodiscard]] static skygate::ephemeris::highprecision::HighPrecisionComputationInput
    input(const EphemerisRequest& request);
    [[nodiscard]] static skygate::ephemeris::highprecision::HighPrecisionComputationInput
    input(const EphemerisRequest& request, const BaseCelestialBody& body);

    [[nodiscard]] static skygate::ephemeris::highprecision::HighPrecisionCalculatorResult calculatorResult();
    [[nodiscard]] static skygate::ephemeris::highprecision::HighPrecisionCalculatorResult
    solarSystemCalculatorResult(const skygate::core::Vector3d& positionAu);

    // Returns a completed transform result whose metadata records the
    // correction implied by the requested frame change.
    [[nodiscard]] static skygate::ephemeris::highprecision::CelestialFrameTransformResult frameTransformResult(
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type sourceFrame,
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type targetFrame,
        const skygate::core::Vector3d& vector
    );

    [[nodiscard]] static skygate::ephemeris::highprecision::CelestialFrameTransformResult transformSingleVector(
        const skygate::ephemeris::highprecision::IFrameTransformer& transformer,
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type sourceFrame,
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type targetFrame,
        const skygate::core::AstronomicalEpoch& epoch,
        const skygate::core::Vector3d& vector
    );
};

}  // namespace skygate::ephemeris::tests
