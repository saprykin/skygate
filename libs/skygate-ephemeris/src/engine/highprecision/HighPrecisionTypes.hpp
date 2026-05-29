#pragma once

#include "BaseCelestialBody.hpp"
#include "EarthOrientationProvider.hpp"
#include "EphemerisRequest.hpp"
#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "engine/EphemerisDatasetInfo.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "math/Vector3d.hpp"
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

struct SolarSystemKernelStateResult {
    std::optional<skygate::core::Vector3d> positionAu;
    std::optional<skygate::core::Vector3d> velocityAuPerDay;
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
    std::optional<skygate::core::Vector3d> observerItrsPositionAu;
};

struct HighPrecisionComputationInput {
    const EphemerisRequest& request;
    const BaseCelestialBody& body;
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState;
    std::size_t bodyIndex = 0U;
};

struct HighPrecisionCalculatorResult {
    std::optional<skygate::core::EquatorialCoordinate> equatorial;
    std::optional<skygate::core::HorizontalCoordinate> horizontal;
    std::optional<skygate::core::Vector3d> observerRelativePositionAu;
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
