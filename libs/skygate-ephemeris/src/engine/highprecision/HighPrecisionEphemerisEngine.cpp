#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include "skygate/core/UtcTimeCodec.hpp"
#include "StringUtilities.hpp"
#include "engine/simple/EquatorialToHorizontalCalculator.hpp"
#include "engine/simple/MoonEquatorialCalculator.hpp"
#include "engine/simple/PlanetEquatorialCalculator.hpp"
#include "engine/simple/SunEquatorialCalculator.hpp"
#include "engine/highprecision/EphemerisMetadataMerge.hpp"
#include "engine/highprecision/EphemerisResultBuilder.hpp"
#include "engine/highprecision/ObserverGeodesy.hpp"
#include "skygate/ephemeris/EphemerisRequestFactory.hpp"

#include <cmath>
#include <bit>
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
constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;
constexpr std::size_t kDirectBodyStateCacheMaxEntries = 8U;

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

[[nodiscard]] std::optional<core::EquatorialCoordinate>
simpleSolarSystemEquatorial(const CelestialBody& body, const core::UtcTimePoint& utcTime)
{
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Sun || body.type == CelestialBodyType::Sun) {
        return SunEquatorialCalculator{}.compute(utcTime);
    }
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Moon || body.type == CelestialBodyType::Moon) {
        return MoonEquatorialCalculator{}.compute(utcTime);
    }
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Planet || body.type == CelestialBodyType::Planet) {
        return PlanetEquatorialCalculator{}.compute(body.id, utcTime);
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<HighPrecisionCalculatorResult> simpleSolarSystemFallbackResult(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& originalResult
)
{
    if (!input.request.options.fallbackToSimpleEngine || originalResult.equatorial.has_value()
        || !originalResult.metadata.hasWarning(EphemerisWarningCode::DataOutOfRange)) {
        return std::nullopt;
    }

    const std::optional<core::EquatorialCoordinate> equatorial =
        simpleSolarSystemEquatorial(input.body, input.request.context.utcTime);
    if (!equatorial.has_value()) {
        return std::nullopt;
    }

    HighPrecisionCalculatorResult fallbackResult = originalResult;
    fallbackResult.equatorial = *equatorial;
    fallbackResult.metadata.status = EphemerisResultStatus::Degraded;
    fallbackResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    fallbackResult.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
    fallbackResult.metadata.dataSourceProvenance =
        fallbackResult.metadata.dataSourceProvenance.empty()
            ? "simple solar-system fallback for out-of-range high-precision kernel"
            : fallbackResult.metadata.dataSourceProvenance
                  + "; simple solar-system fallback for out-of-range high-precision kernel";
    fallbackResult.metadata.appliedCorrections = EphemerisCorrectionFlags::Geometric;
    if (requestsTopocentricState(input.request) && input.request.context.observer.isValid()) {
        fallbackResult.horizontal = EquatorialToHorizontalCalculator::compute(
            *equatorial, input.request.context.observer, input.request.context.utcTime
        );
        fallbackResult.metadata.appliedCorrections |= EphemerisCorrectionFlags::DiurnalParallax;
    }

    return fallbackResult;
}

[[nodiscard]] bool sameDoubleIdentity(const double lhs, const double rhs) noexcept
{
    return std::bit_cast<std::uint64_t>(lhs) == std::bit_cast<std::uint64_t>(rhs);
}

[[nodiscard]] bool sameEpoch(const AstronomicalEpoch& lhs, const AstronomicalEpoch& rhs) noexcept
{
    return sameDoubleIdentity(lhs.julianDatePart1, rhs.julianDatePart1)
           && sameDoubleIdentity(lhs.julianDatePart2, rhs.julianDatePart2) && lhs.timeScale == rhs.timeScale;
}

[[nodiscard]] bool sameObserver(const core::GeoLocation& lhs, const core::GeoLocation& rhs) noexcept
{
    return sameDoubleIdentity(lhs.latitudeDeg, rhs.latitudeDeg)
           && sameDoubleIdentity(lhs.longitudeDeg, rhs.longitudeDeg)
           && sameDoubleIdentity(lhs.elevationMeters, rhs.elevationMeters);
}

[[nodiscard]] bool sameOptions(const EphemerisEngineOptions& lhs, const EphemerisEngineOptions& rhs) noexcept
{
    return lhs.engineKind == rhs.engineKind && lhs.correctionFlags == rhs.correctionFlags
           && lhs.fallbackToSimpleEngine == rhs.fallbackToSimpleEngine
           && lhs.enableAtmosphericRefraction == rhs.enableAtmosphericRefraction
           && sameDoubleIdentity(lhs.atmosphericPressureHpa, rhs.atmosphericPressureHpa)
           && sameDoubleIdentity(lhs.atmosphericTemperatureC, rhs.atmosphericTemperatureC)
           && sameDoubleIdentity(lhs.relativeHumidity, rhs.relativeHumidity)
           && sameDoubleIdentity(lhs.observingWavelengthMicrometers, rhs.observingWavelengthMicrometers);
}

[[nodiscard]] bool sameRequest(const EphemerisRequest& lhs, const EphemerisRequest& rhs) noexcept
{
    return sameEpoch(lhs.epoch, rhs.epoch)
           && core::UtcTimeCodec::toEpochMicros(lhs.context.utcTime)
                  == core::UtcTimeCodec::toEpochMicros(rhs.context.utcTime)
           && sameObserver(lhs.context.observer, rhs.context.observer) && sameOptions(lhs.options, rhs.options);
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
            if (isSolarSystemBody((*m_bodies)[bodyIndex])) {
                storeDirectBodyState(request, bodyIndex, snapshot.states[bodyIndex]);
            }
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
        if (isSolarSystemBody((*m_bodies)[bodyIndex])) {
            storeDirectBodyState(request, bodyIndex, snapshot.states[bodyIndex]);
        }
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
            if (isSolarSystemBody(body)) {
                if (std::optional<CelestialBodyState> cachedState = findDirectBodyState(request, bodyIndex);
                    cachedState.has_value()) {
                    return cachedState;
                }
                CelestialBodyState state = computeStateForBody(request, bodyIndex, buildPreparedRequestState(request));
                storeDirectBodyState(request, bodyIndex, state);
                return state;
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
    }

    return std::nullopt;
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    if (bodyIndex >= m_bodies->size() || bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }

    const CelestialBody& body = (*m_bodies)[bodyIndex];
    if (isSolarSystemBody(body)) {
        if (std::optional<CelestialBodyState> cachedState = findDirectBodyState(request, bodyIndex);
            cachedState.has_value()) {
            return cachedState;
        }
        CelestialBodyState state = computeStateForBody(request, bodyIndex, buildPreparedRequestState(request));
        storeDirectBodyState(request, bodyIndex, state);
        return state;
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

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::findDirectBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    const std::scoped_lock lock(m_directBodyStateCacheMutex);
    for (auto cacheEntry = m_directBodyStateCache.rbegin(); cacheEntry != m_directBodyStateCache.rend(); ++cacheEntry) {
        if (cacheEntry->bodyIndex == bodyIndex && sameRequest(cacheEntry->request, request)) {
            return cacheEntry->state;
        }
    }

    return std::nullopt;
}

void HighPrecisionEphemerisEngine::storeDirectBodyState(
    const EphemerisRequest& request, const std::size_t bodyIndex, const CelestialBodyState& state
) const
{
    const std::scoped_lock lock(m_directBodyStateCacheMutex);
    for (DirectBodyStateCacheEntry& cacheEntry : m_directBodyStateCache) {
        if (cacheEntry.bodyIndex == bodyIndex && sameRequest(cacheEntry.request, request)) {
            cacheEntry.state = state;
            return;
        }
    }

    m_directBodyStateCache.push_back(
        DirectBodyStateCacheEntry{.request = request, .bodyIndex = bodyIndex, .state = state}
    );
    while (m_directBodyStateCache.size() > kDirectBodyStateCacheMaxEntries) {
        m_directBodyStateCache.pop_front();
    }
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
        if (std::optional<HighPrecisionCalculatorResult> fallbackResult =
                simpleSolarSystemFallbackResult(input, calculatorResult);
            fallbackResult.has_value()) {
            return builder.buildState(input, *fallbackResult);
        }
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
