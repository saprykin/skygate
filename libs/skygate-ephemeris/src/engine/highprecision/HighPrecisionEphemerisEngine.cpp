#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include "StringUtilities.hpp"
#include "engine/highprecision/EphemerisMetadataMerge.hpp"
#include "engine/highprecision/EphemerisResultBuilder.hpp"
#include "skygate/ephemeris/EphemerisRequestFactory.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris::highprecision {

namespace {

constexpr std::string_view kHighPrecisionEngineName = "High-precision ephemeris engine";
constexpr double kAstronomicalUnitMeters = 149'597'870'700.0;
constexpr double kWgs84EquatorialRadiusMeters = 6'378'137.0;
constexpr double kWgs84Flattening = 1.0 / 298.257223563;
constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;

[[nodiscard]] bool hasValidEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] bool isSolarSystemBody(const CelestialBody& body) noexcept
{
    return body.ephemerisSource == CelestialBodyEphemerisSource::Sun
           || body.ephemerisSource == CelestialBodyEphemerisSource::Moon
           || body.ephemerisSource == CelestialBodyEphemerisSource::Planet || body.type == CelestialBodyType::Sun
           || body.type == CelestialBodyType::Moon || body.type == CelestialBodyType::Planet;
}

[[nodiscard]] bool isCatalogStarBody(const CelestialBody& body) noexcept
{
    return body.ephemerisSource == CelestialBodyEphemerisSource::Star
           || body.ephemerisSource == CelestialBodyEphemerisSource::FixedEquatorial
           || body.type == CelestialBodyType::Star;
}

[[nodiscard]] bool requestsApparentPlaceProcessing(const EphemerisRequest& request) noexcept
{
    return request.options.correctionFlags != EphemerisCorrectionFlags::NoCorrections;
}

[[nodiscard]] bool requestsAnnualParallaxState(const EphemerisRequest& request) noexcept
{
    return hasCorrectionFlag(request.options.correctionFlags, EphemerisCorrectionFlags::AnnualParallax);
}

[[nodiscard]] bool requestsTopocentricState(const EphemerisRequest& request) noexcept
{
    return hasCorrectionFlag(request.options.correctionFlags, EphemerisCorrectionFlags::DiurnalParallax);
}

[[nodiscard]] std::optional<SolarSystemKernelVector> observerItrsPositionAu(const core::GeoLocation& observer) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }

    const double latitudeRad = observer.latitudeDeg * 3.141592653589793238462643383279502884 / 180.0;
    const double longitudeRad = observer.longitudeDeg * 3.141592653589793238462643383279502884 / 180.0;
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);
    const double firstEccentricitySquared = kWgs84Flattening * (2.0 - kWgs84Flattening);
    const double primeVerticalRadius =
        kWgs84EquatorialRadiusMeters / std::sqrt(1.0 - firstEccentricitySquared * sinLatitude * sinLatitude);

    const double xMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * cosLongitude;
    const double yMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * sinLongitude;
    const double zMeters =
        (primeVerticalRadius * (1.0 - firstEccentricitySquared) + observer.elevationMeters) * sinLatitude;
    return SolarSystemKernelVector{
        .xAu = xMeters / kAstronomicalUnitMeters,
        .yAu = yMeters / kAstronomicalUnitMeters,
        .zAu = zMeters / kAstronomicalUnitMeters,
    };
}

void mergeKernelEpochTimeScaleMetadata(
    EphemerisResultMetadata& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    constexpr std::uint32_t kTdbApproximationWarning =
        timeScaleConversionWarningMask(TimeScaleConversionWarningCode::TdbApproximationApplied);
    if (conversion.status == TimeScaleConversionStatus::Degraded
        && (conversion.warningCodeMask & ~kTdbApproximationWarning) == 0U) {
        return;
    }

    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);
}

class DefaultApparentPlaceCalculator final : public IApparentPlaceCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput&, const HighPrecisionCalculatorResult& calculatorResult) const override
    {
        return calculatorResult;
    }
};

[[nodiscard]] const IApparentPlaceCalculator&
apparentPlaceCalculator(const HighPrecisionEphemerisEngineDependencies& dependencies)
{
    static const DefaultApparentPlaceCalculator kDefaultCalculator;
    if (dependencies.apparentPlaceCalculator != nullptr) {
        return *dependencies.apparentPlaceCalculator;
    }

    return kDefaultCalculator;
}

[[nodiscard]] const IEphemerisResultBuilder& resultBuilder(const HighPrecisionEphemerisEngineDependencies& dependencies)
{
    static const EphemerisResultBuilder kDefaultBuilder;
    if (dependencies.resultBuilder != nullptr) {
        return *dependencies.resultBuilder;
    }

    return kDefaultBuilder;
}

[[nodiscard]] HighPrecisionCalculatorResult applyApparentPlaceIfRequested(
    const HighPrecisionEphemerisEngineDependencies& dependencies,
    const EphemerisRequest& request,
    const HighPrecisionComputationInput& input,
    const HighPrecisionCalculatorResult& calculatorResult
)
{
    if (!requestsApparentPlaceProcessing(request)) {
        return calculatorResult;
    }

    return apparentPlaceCalculator(dependencies).apply(input, calculatorResult);
}

[[nodiscard]] std::optional<AstronomicalEpoch> kernelEpochForSolarSystemState(
    EphemerisResultMetadata& metadata,
    const EphemerisRequest& request,
    const PreparedEphemerisRequestState* preparedState,
    const std::shared_ptr<const ITimeScaleService>& timeScaleService
)
{
    if (request.epoch.timeScale == TimeScale::Tdb) {
        return normalizedAstronomicalEpoch(request.epoch);
    }
    if (preparedState != nullptr && preparedState->tdbKernelEpoch.has_value()) {
        EphemerisMetadataMerger::merge(metadata, preparedState->tdbKernelEpochMetadata);
        return preparedState->tdbKernelEpoch;
    }
    if (timeScaleService == nullptr) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return std::nullopt;
    }

    const TimeScaleConversionResult conversion = timeScaleService->convert(request.epoch, TimeScale::Tdb);
    mergeKernelEpochTimeScaleMetadata(metadata, conversion);
    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return normalizedAstronomicalEpoch(conversion.epoch);
}

[[nodiscard]] std::vector<StarAstrometryBatchResult> applyApparentPlaceBatchIfRequested(
    const HighPrecisionEphemerisEngineDependencies& dependencies,
    const EphemerisRequest& request,
    const std::span<const CelestialBody> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedState
)
{
    if (!requestsApparentPlaceProcessing(request)) {
        return {calculatorResults.begin(), calculatorResults.end()};
    }

    return apparentPlaceCalculator(dependencies)
        .applyBatch(request, bodies, calculatorResults, std::move(preparedState));
}

}  // namespace

HighPrecisionEphemerisEngine::HighPrecisionEphemerisEngine(
    const std::span<const CelestialBody> bodies,
    EphemerisEngineOptions engineOptions,
    HighPrecisionEphemerisEngineDependencies dependencies
)
    : m_bodies(std::make_shared<const std::vector<CelestialBody>>(bodies.begin(), bodies.end())),
      m_catalogStarAstrometryArrays(*m_bodies), m_options(engineOptions), m_dependencies(std::move(dependencies))
{
    m_options.engineKind = EphemerisEngineKind::HighPrecision;
}

EphemerisEngineKind HighPrecisionEphemerisEngine::kind() const noexcept
{
    return EphemerisEngineKind::HighPrecision;
}

std::string_view HighPrecisionEphemerisEngine::name() const noexcept
{
    return kHighPrecisionEngineName;
}

EphemerisCapabilities HighPrecisionEphemerisEngine::capabilities() const noexcept
{
    EphemerisCapabilities engineCapabilities;
    engineCapabilities.engineKind = EphemerisEngineKind::HighPrecision;
    engineCapabilities.supportedCorrections = m_options.correctionFlags;
    engineCapabilities.supportsSolarSystemBodies = m_dependencies.solarSystemStateCalculator != nullptr;
    engineCapabilities.supportsCatalogStars = m_dependencies.starAstrometryCalculator != nullptr;
    engineCapabilities.supportsTopocentricPositions = m_dependencies.earthOrientationProvider != nullptr;
    engineCapabilities.supportsAtmosphericRefraction = m_dependencies.atmosphericRefractionCalculator != nullptr;
    engineCapabilities.supportsExtendedHistoricalRange = !m_dependencies.dataSetInfo.dateRanges.empty();
    return engineCapabilities;
}

std::span<const EphemerisDateRange> HighPrecisionEphemerisEngine::supportedDateRanges() const noexcept
{
    return m_dependencies.dataSetInfo.dateRanges;
}

EphemerisDataSetInfo HighPrecisionEphemerisEngine::dataSetInfo() const
{
    return m_dependencies.dataSetInfo;
}

EphemerisEngineOptions HighPrecisionEphemerisEngine::options() const noexcept
{
    return m_options;
}

SkySnapshot HighPrecisionEphemerisEngine::compute(const EphemerisRequest& request) const
{
    if (m_dependencies.computationCache != nullptr) {
        if (std::optional<SkySnapshot> cachedSnapshot =
                m_dependencies.computationCache->findSnapshot(request, *m_bodies, m_dependencies.dataSetInfo);
            cachedSnapshot.has_value()) {
            return *cachedSnapshot;
        }
    }

    const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = preparedRequestState(request);
    SkySnapshot snapshot = computeUncached(request, preparedState);
    if (m_dependencies.computationCache != nullptr) {
        m_dependencies.computationCache->storeSnapshot(request, *m_bodies, m_dependencies.dataSetInfo, snapshot);
    }
    return snapshot;
}

SkySnapshot HighPrecisionEphemerisEngine::computeUncached(
    const EphemerisRequest& request, std::shared_ptr<const PreparedEphemerisRequestState> preparedState
) const
{
    SkySnapshot snapshot;
    snapshot.context = request.context;
    snapshot.catalogBodies = m_bodies;
    snapshot.states.resize(m_bodies->size());

    const IEphemerisResultBuilder& builder = resultBuilder(m_dependencies);
    if (!hasValidEpoch(request.epoch)) {
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            const HighPrecisionComputationInput input{
                .request = request,
                .body = (*m_bodies)[bodyIndex],
                .preparedRequestState = preparedState,
                .bodyIndex = bodyIndex,
            };
            snapshot.states[bodyIndex] = builder.buildFailedState(input);
        }
        return snapshot;
    }

    std::vector<std::uint8_t> batchFilledStates(m_bodies->size(), 0U);
    const IStarAstrometryCalculator* starAstrometryCalculator = m_dependencies.starAstrometryCalculator.get();
    if (starAstrometryCalculator != nullptr && !m_catalogStarAstrometryArrays.empty()) {
        const std::vector<StarAstrometryBatchResult> batchResults =
            starAstrometryCalculator->calculateBatch(request, m_catalogStarAstrometryArrays, preparedState);
        const std::vector<StarAstrometryBatchResult> apparentBatchResults =
            applyApparentPlaceBatchIfRequested(m_dependencies, request, *m_bodies, batchResults, preparedState);
        for (const StarAstrometryBatchResult& batchResult : apparentBatchResults) {
            if (batchResult.bodyIndex >= m_bodies->size()
                || batchResult.bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
                continue;
            }

            const HighPrecisionComputationInput input{
                .request = request,
                .body = (*m_bodies)[batchResult.bodyIndex],
                .preparedRequestState = preparedState,
                .bodyIndex = batchResult.bodyIndex,
            };
            snapshot.states[batchResult.bodyIndex] = builder.buildState(input, batchResult.result);
            batchFilledStates[batchResult.bodyIndex] = 1U;
        }
    }

    for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        if (batchFilledStates[bodyIndex] != 0U) {
            continue;
        }
        snapshot.states[bodyIndex] = computeStateForBody(request, bodyIndex, preparedState);
    }

    return snapshot;
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
{
    if (bodyId.empty()) {
        return std::nullopt;
    }

    for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        const CelestialBody& body = (*m_bodies)[bodyIndex];
        if (strings::equalsIgnoreAsciiCase(body.id, bodyId)) {
            if (m_dependencies.computationCache != nullptr) {
                if (std::optional<SkySnapshot> cachedSnapshot =
                        m_dependencies.computationCache->findSnapshot(request, *m_bodies, m_dependencies.dataSetInfo);
                    cachedSnapshot.has_value() && bodyIndex < cachedSnapshot->states.size()) {
                    return cachedSnapshot->states[bodyIndex];
                }
                if (std::optional<CelestialBodyState> cachedBodyState = m_dependencies.computationCache->findBodyState(
                        request, *m_bodies, m_dependencies.dataSetInfo, bodyIndex
                    );
                    cachedBodyState.has_value()) {
                    return cachedBodyState;
                }
            }
            CelestialBodyState state = computeStateForBody(request, bodyIndex, preparedRequestState(request));
            if (m_dependencies.computationCache != nullptr) {
                m_dependencies.computationCache->storeBodyState(
                    request, *m_bodies, m_dependencies.dataSetInfo, bodyIndex, state
                );
            }
            return state;
        }
    }

    return std::nullopt;
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    if (bodyIndex >= m_bodies->size() || bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }

    if (m_dependencies.computationCache != nullptr) {
        if (std::optional<SkySnapshot> cachedSnapshot =
                m_dependencies.computationCache->findSnapshot(request, *m_bodies, m_dependencies.dataSetInfo);
            cachedSnapshot.has_value() && bodyIndex < cachedSnapshot->states.size()) {
            return cachedSnapshot->states[bodyIndex];
        }
        if (std::optional<CelestialBodyState> cachedBodyState = m_dependencies.computationCache->findBodyState(
                request, *m_bodies, m_dependencies.dataSetInfo, bodyIndex
            );
            cachedBodyState.has_value()) {
            return cachedBodyState;
        }
    }

    CelestialBodyState state = computeStateForBody(request, bodyIndex, preparedRequestState(request));
    if (m_dependencies.computationCache != nullptr) {
        m_dependencies.computationCache->storeBodyState(
            request, *m_bodies, m_dependencies.dataSetInfo, bodyIndex, state
        );
    }
    return state;
}

SkySnapshot HighPrecisionEphemerisEngine::compute(const core::SkyContext& context) const
{
    return compute(makeCompatibilityRequest(context));
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const core::SkyContext& context, const std::string_view bodyId) const
{
    return computeBodyState(makeCompatibilityRequest(context), bodyId);
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const core::SkyContext& context, const std::uint32_t bodyIndex) const
{
    return computeBodyState(makeCompatibilityRequest(context), static_cast<std::size_t>(bodyIndex));
}

EphemerisRequest HighPrecisionEphemerisEngine::makeCompatibilityRequest(const core::SkyContext& context) const noexcept
{
    return EphemerisRequestFactory::fromContext(context, m_options);
}

std::shared_ptr<const PreparedEphemerisRequestState>
HighPrecisionEphemerisEngine::preparedRequestState(const EphemerisRequest& request) const
{
    if (m_dependencies.computationCache != nullptr) {
        if (std::shared_ptr<const PreparedEphemerisRequestState> cachedState =
                m_dependencies.computationCache->findPreparedRequestState(
                    request, *m_bodies, m_dependencies.dataSetInfo
                );
            cachedState != nullptr) {
            return cachedState;
        }
    }

    std::shared_ptr<const PreparedEphemerisRequestState> preparedState = buildPreparedRequestState(request);
    if (m_dependencies.computationCache != nullptr) {
        m_dependencies.computationCache->storePreparedRequestState(
            request, *m_bodies, m_dependencies.dataSetInfo, preparedState
        );
    }
    return preparedState;
}

std::shared_ptr<const PreparedEphemerisRequestState>
HighPrecisionEphemerisEngine::buildPreparedRequestState(const EphemerisRequest& request) const
{
    auto preparedState = std::make_shared<PreparedEphemerisRequestState>();
    {
        if (request.epoch.timeScale == TimeScale::Tdb) {
            preparedState->tdbKernelEpoch = normalizedAstronomicalEpoch(request.epoch);
        } else if (m_dependencies.timeScaleService == nullptr) {
            preparedState->tdbKernelEpochMetadata.status = EphemerisResultStatus::Degraded;
            preparedState->tdbKernelEpochMetadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        } else {
            const TimeScaleConversionResult conversion =
                m_dependencies.timeScaleService->convert(request.epoch, TimeScale::Tdb);
            mergeKernelEpochTimeScaleMetadata(preparedState->tdbKernelEpochMetadata, conversion);
            if (conversion.isSuccess()) {
                preparedState->tdbKernelEpoch = normalizedAstronomicalEpoch(conversion.epoch);
            }
        }

        if (requestsAnnualParallaxState(request) && preparedState->tdbKernelEpoch.has_value()
            && m_dependencies.calcephKernelProvider != nullptr) {
            preparedState->annualParallaxEarthState = m_dependencies.calcephKernelProvider->computeGeometricState(
                *preparedState->tdbKernelEpoch, kNaifEarth, kNaifSolarSystemBarycenter
            );
        }
    }

    if (requestsTopocentricState(request) && m_dependencies.timeScaleService != nullptr) {
        preparedState->topocentricStatePrepared = true;
        preparedState->observerItrsPositionAu = observerItrsPositionAu(request.context.observer);
        const TimeScaleConversionResult utcConversion =
            m_dependencies.timeScaleService->convert(request.epoch, TimeScale::Utc);
        EphemerisMetadataMerger::mergeTimeScale(preparedState->topocentricMetadata, utcConversion);
        if (!utcConversion.isSuccess()) {
            preparedState->topocentricStateAvailable = false;
            EphemerisMetadataMerger::markCorrectionUnavailable(
                preparedState->topocentricMetadata, EphemerisCorrectionFlags::EarthOrientation
            );
        } else {
            preparedState->earthOrientationSample = sampleEarthOrientation(
                m_dependencies.earthOrientationProvider,
                utcConversion.epoch,
                EarthOrientationSampleOptions{
                    .allowOutOfRangeNearestSampleFallback = true,
                    .allowMissingDataZeroFallback = true,
                    .degradePredictedData = false,
                }
            );
            EphemerisMetadataMerger::mergeEarthOrientation(
                preparedState->topocentricMetadata, *preparedState->earthOrientationSample
            );
            if (!preparedState->earthOrientationSample->isSuccess()) {
                preparedState->topocentricStateAvailable = false;
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    preparedState->topocentricMetadata, EphemerisCorrectionFlags::EarthOrientation
                );
            }
        }
    }

    return preparedState;
}

CelestialBodyState HighPrecisionEphemerisEngine::computeStateForBody(
    const EphemerisRequest& request,
    const std::size_t bodyIndex,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedState
) const
{
    const CelestialBody& body = (*m_bodies)[bodyIndex];
    const HighPrecisionComputationInput input{
        .request = request,
        .body = body,
        .preparedRequestState = std::move(preparedState),
        .bodyIndex = bodyIndex,
    };
    const IEphemerisResultBuilder& builder = resultBuilder(m_dependencies);

    if (!hasValidEpoch(request.epoch)) {
        return builder.buildFailedState(input);
    }

    const ISolarSystemStateCalculator* solarSystemCalculator = m_dependencies.solarSystemStateCalculator.get();
    if (isSolarSystemBody(body) && solarSystemCalculator != nullptr) {
        HighPrecisionCalculatorResult kernelEpochMetadata;
        const std::optional<AstronomicalEpoch> kernelEpoch = kernelEpochForSolarSystemState(
            kernelEpochMetadata.metadata, request, input.preparedRequestState.get(), m_dependencies.timeScaleService
        );
        if (!kernelEpoch.has_value()) {
            return builder.buildState(input, kernelEpochMetadata);
        }
        EphemerisRequest kernelRequest = request;
        kernelRequest.epoch = *kernelEpoch;
        const HighPrecisionComputationInput kernelInput{
            .request = kernelRequest,
            .body = body,
            .preparedRequestState = input.preparedRequestState,
            .bodyIndex = bodyIndex,
        };
        HighPrecisionCalculatorResult calculatorResult = solarSystemCalculator->calculate(kernelInput);
        EphemerisMetadataMerger::merge(calculatorResult.metadata, kernelEpochMetadata.metadata);
        HighPrecisionCalculatorResult apparentResult =
            applyApparentPlaceIfRequested(m_dependencies, request, input, calculatorResult);
        return builder.buildState(input, apparentResult);
    }

    const IStarAstrometryCalculator* starAstrometryCalculator = m_dependencies.starAstrometryCalculator.get();
    if (isCatalogStarBody(body) && starAstrometryCalculator != nullptr) {
        HighPrecisionCalculatorResult calculatorResult = starAstrometryCalculator->calculate(input);
        HighPrecisionCalculatorResult apparentResult =
            applyApparentPlaceIfRequested(m_dependencies, request, input, calculatorResult);
        return builder.buildState(input, apparentResult);
    }

    return builder.buildUnsupportedState(input);
}

}  // namespace skygate::ephemeris::highprecision
