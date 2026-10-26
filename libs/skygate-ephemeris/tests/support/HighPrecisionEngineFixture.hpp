#pragma once

#include "CelestialBodyCatalog.hpp"
#include "EphemerisRequest.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "TestCalcephKernel.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/IApparentPlaceCalculator.hpp"
#include "engine/highprecision/IEphemerisComputationCache.hpp"
#include "engine/highprecision/IEphemerisResultBuilder.hpp"
#include "engine/highprecision/IStarAstrometryCalculator.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace skygate::ephemeris::tests {

// Builds catalog bodies, requests, engine dependency bundles, and collaborator
// inputs shared by the high-precision engine test suites.
class HighPrecisionEngineFixture final {
public:
    [[nodiscard]] static OwnGalaxyCelestialBody sunBody();
    [[nodiscard]] static OwnGalaxyCelestialBody starBody();
    [[nodiscard]] static OwnGalaxyCelestialBody
    fixedStarBody(std::string id, double rightAscensionHours, double declinationDeg);
    [[nodiscard]] static CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody> bodies);
    [[nodiscard]] static skygate::core::ObservationContext context();
    [[nodiscard]] static EphemerisRequest request();

    [[nodiscard]] static skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies(
        std::shared_ptr<skygate::ephemeris::highprecision::ISolarSystemStateCalculator> solarSystemCalculator = {},
        std::shared_ptr<skygate::ephemeris::highprecision::IStarAstrometryCalculator> starAstrometryCalculator = {},
        std::shared_ptr<skygate::ephemeris::highprecision::IApparentPlaceCalculator> apparentPlaceCalculator = {},
        std::shared_ptr<skygate::ephemeris::highprecision::IEphemerisResultBuilder> resultBuilder = {},
        std::shared_ptr<skygate::ephemeris::highprecision::IEphemerisComputationCache> computationCache = {}
    );

    [[nodiscard]] static skygate::ephemeris::highprecision::HighPrecisionCalculatorResult calculatorResult(
        const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input,
        double rightAscensionHours,
        double declinationDeg,
        std::string_view provenance
    );

    [[nodiscard]] static std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider>
    earthOrientationProvider(const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch = std::nullopt);

    [[nodiscard]] static std::shared_ptr<TestCalcephKernel> longRangeFallbackKernel();
};

}  // namespace skygate::ephemeris::tests
