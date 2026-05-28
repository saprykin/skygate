#pragma once

#include "BaseCelestialBody.hpp"
#include "EarthOrientationProvider.hpp"
#include "EphemerisRequest.hpp"
#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "engine/EphemerisDatasetInfo.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstddef>
#include <memory>
#include <optional>

namespace skygate::ephemeris {
class ITimeScaleService;
class IEarthOrientationProvider;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

// Forward declarations for interface classes referenced in dependencies.
class ICalcephKernelProvider;
class ISolarSystemStateCalculator;
class IStarAstrometryCalculator;
class IApparentPlaceCalculator;
class IAtmosphericRefractionCalculator;
class IFrameTransformer;
class IEphemerisResultBuilder;
class IEphemerisComputationCache;

struct SolarSystemKernelVector {
    double xAu = 0.0;
    double yAu = 0.0;
    double zAu = 0.0;
};

struct SolarSystemKernelStateResult {
    std::optional<SolarSystemKernelVector> positionAu;
    std::optional<SolarSystemKernelVector> velocityAuPerDay;
    EphemerisEngineQueryResult metadata;
};

struct PreparedEphemerisRequestState {
    EphemerisEngineQueryResult tdbKernelEpochMetadata;
    std::optional<AstronomicalEpoch> tdbKernelEpoch;
    std::optional<SolarSystemKernelStateResult> annualParallaxEarthState;

    bool topocentricStatePrepared = false;
    bool topocentricStateAvailable = true;
    EphemerisEngineQueryResult topocentricMetadata;
    std::optional<EarthOrientationSample> earthOrientationSample;
    std::optional<SolarSystemKernelVector> observerItrsPositionAu;
};

struct HighPrecisionComputationInput {
    const EphemerisRequest& request;
    const BaseCelestialBody& body;
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState;
    std::size_t bodyIndex = 0U;
};

struct HighPrecisionCalculatorResult {
    std::optional<core::EquatorialCoordinate> equatorial;
    std::optional<core::HorizontalCoordinate> horizontal;
    std::optional<SolarSystemKernelVector> observerRelativePositionAu;
    EphemerisEngineQueryResult metadata;
};

struct StarAstrometryBatchResult {
    std::size_t bodyIndex = 0U;
    HighPrecisionCalculatorResult result;
};

struct HighPrecisionEphemerisEngineDependencies {
    std::shared_ptr<const ICalcephKernelProvider> calcephKernelProvider;
    std::shared_ptr<const ISolarSystemStateCalculator> solarSystemStateCalculator;
    std::shared_ptr<const IStarAstrometryCalculator> starAstrometryCalculator;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
    std::shared_ptr<const IFrameTransformer> frameTransformer;
    std::shared_ptr<const IApparentPlaceCalculator> apparentPlaceCalculator;
    std::shared_ptr<const IAtmosphericRefractionCalculator> atmosphericRefractionCalculator;
    std::shared_ptr<const IEphemerisResultBuilder> resultBuilder;
    std::shared_ptr<const IEphemerisComputationCache> computationCache;
    EphemerisDatasetInfo dataSetInfo;
};

}  // namespace skygate::ephemeris::highprecision
