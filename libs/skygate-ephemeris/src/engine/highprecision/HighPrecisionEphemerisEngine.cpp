#include "HighPrecisionEphemerisEngine.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "EphemerisRequestFactory.hpp"
#include "EphemerisResultBuilder.hpp"
#include "IApparentPlaceCalculator.hpp"
#include "ICalcephKernelProvider.hpp"
#include "IEphemerisComputationCache.hpp"
#include "IEphemerisResultBuilder.hpp"
#include "ISolarSystemStateCalculator.hpp"
#include "IStarAstrometryCalculator.hpp"
#include "ObserverGeodesy.hpp"
#include "StringUtilities.hpp"
#include "UtcTimeCodec.hpp"
#include "engine/simple/EquatorialToHorizontalCalculator.hpp"
#include "engine/simple/MoonEquatorialCalculator.hpp"
#include "engine/simple/PlanetEquatorialCalculator.hpp"
#include "engine/simple/SunEquatorialCalculator.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
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

[[nodiscard]] bool isSolarSystemBody(const BaseCelestialBody& body) noexcept
{
    return body.kind == BaseCelestialBody::Kind::Sun || body.kind == BaseCelestialBody::Kind::Moon
           || body.kind == BaseCelestialBody::Kind::Planet;
}

[[nodiscard]] bool isCatalogStarBody(const BaseCelestialBody& body) noexcept
{
    return body.kind == BaseCelestialBody::Kind::Star || body.fixedEquatorialValue().has_value();
}

[[nodiscard]] bool requestsApparentPlaceProcessing(const EphemerisRequest& request) noexcept
{
    return request.options.correctionFlags() != EphemerisCorrectionFlags::noCorrections();
}

[[nodiscard]] bool requestsAnnualParallaxState(const EphemerisRequest& request) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(
        request.options.correctionFlags(), EphemerisCorrectionFlags::annualParallax()
    );
}

[[nodiscard]] bool requestsTopocentricState(const EphemerisRequest& request) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(
        request.options.correctionFlags(), EphemerisCorrectionFlags::diurnalParallax()
    );
}

[[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
simpleSolarSystemEquatorial(const BaseCelestialBody& body, const skygate::core::UtcTimePoint& utcTime)
{
    if (body.kind == BaseCelestialBody::Kind::Sun || body.kind == BaseCelestialBody::Kind::Sun) {
        return SunEquatorialCalculator{}.compute(utcTime);
    }
    if (body.kind == BaseCelestialBody::Kind::Moon || body.kind == BaseCelestialBody::Kind::Moon) {
        return MoonEquatorialCalculator{}.compute(utcTime);
    }
    if (body.kind == BaseCelestialBody::Kind::Planet || body.kind == BaseCelestialBody::Kind::Planet) {
        return PlanetEquatorialCalculator{}.compute(body.id, utcTime);
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<HighPrecisionCalculatorResult> simpleSolarSystemFallbackResult(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& originalResult
)
{
    if (!input.request.options.fallbackToSimpleEngine() || originalResult.equatorial.has_value()
        || !originalResult.metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange)) {
        return std::nullopt;
    }

    const std::optional<skygate::core::EquatorialCoordinate> equatorial =
        simpleSolarSystemEquatorial(input.body, input.request.context.utcTime);
    if (!equatorial.has_value()) {
        return std::nullopt;
    }

    HighPrecisionCalculatorResult fallbackResult = originalResult;
    fallbackResult.equatorial = *equatorial;
    fallbackResult.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    fallbackResult.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    fallbackResult.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    fallbackResult.metadata.dataSourceProvenance =
        fallbackResult.metadata.dataSourceProvenance.empty()
            ? "simple solar-system fallback for out-of-range high-precision kernel"
            : fallbackResult.metadata.dataSourceProvenance
                  + "; simple solar-system fallback for out-of-range high-precision kernel";
    fallbackResult.metadata.appliedCorrections = EphemerisCorrectionFlags::geometric();
    if (requestsTopocentricState(input.request) && input.request.context.observer.isValid()) {
        fallbackResult.horizontal = EquatorialToHorizontalCalculator::compute(
            *equatorial, input.request.context.observer, input.request.context.utcTime
        );
        fallbackResult.metadata.appliedCorrections |= EphemerisCorrectionFlags::diurnalParallax();
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

[[nodiscard]] bool sameObserver(const skygate::core::GeoLocation& lhs, const skygate::core::GeoLocation& rhs) noexcept
{
    return sameDoubleIdentity(lhs.latitudeDeg, rhs.latitudeDeg)
           && sameDoubleIdentity(lhs.longitudeDeg, rhs.longitudeDeg)
           && sameDoubleIdentity(lhs.elevationMeters, rhs.elevationMeters);
}

[[nodiscard]] bool sameOptions(const EphemerisEngineOptions& lhs, const EphemerisEngineOptions& rhs) noexcept
{
    return lhs == rhs;
}

[[nodiscard]] bool sameRequest(const EphemerisRequest& lhs, const EphemerisRequest& rhs) noexcept
{
    return sameEpoch(lhs.epoch, rhs.epoch)
           && skygate::core::UtcTimeCodec::toEpochMicros(lhs.context.utcTime)
                  == skygate::core::UtcTimeCodec::toEpochMicros(rhs.context.utcTime)
           && sameObserver(lhs.context.observer, rhs.context.observer) && sameOptions(lhs.options, rhs.options);
}

void mergeKernelEpochTimeScaleMetadata(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
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

[[nodiscard]] HighPrecisionCalculatorResult missingApparentPlaceCalculatorResult(
    HighPrecisionCalculatorResult calculatorResult, const EphemerisCorrectionFlags requestedCorrections
)
{
    calculatorResult.equatorial.reset();
    calculatorResult.horizontal.reset();
    calculatorResult.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    calculatorResult.metadata.addUnavailableCorrection(requestedCorrections);
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    return calculatorResult;
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

    if (dependencies.apparentPlaceCalculator == nullptr) {
        return missingApparentPlaceCalculatorResult(calculatorResult, request.options.correctionFlags());
    }

    return dependencies.apparentPlaceCalculator->apply(input, calculatorResult);
}

[[nodiscard]] std::optional<AstronomicalEpoch> kernelEpochForSolarSystemState(
    EphemerisEngineQueryResult& metadata,
    const EphemerisRequest& request,
    const PreparedEphemerisRequestState* preparedState,
    const std::shared_ptr<const ITimeScaleService>& timeScaleService
)
{
    if (request.epoch.timeScale == TimeScale::Tdb) {
        return request.epoch.normalized();
    }
    if (preparedState != nullptr && preparedState->tdbKernelEpoch.has_value()) {
        EphemerisMetadataMerger::merge(metadata, preparedState->tdbKernelEpochMetadata);
        return preparedState->tdbKernelEpoch;
    }
    if (timeScaleService == nullptr) {
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return std::nullopt;
    }

    const TimeScaleConversionResult conversion = timeScaleService->convert(request.epoch, TimeScale::Tdb);
    mergeKernelEpochTimeScaleMetadata(metadata, conversion);
    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return conversion.epoch.normalized();
}

[[nodiscard]] std::vector<StarAstrometryBatchResult> applyApparentPlaceBatchIfRequested(
    const HighPrecisionEphemerisEngineDependencies& dependencies,
    const EphemerisRequest& request,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedState
)
{
    if (!requestsApparentPlaceProcessing(request)) {
        return {calculatorResults.begin(), calculatorResults.end()};
    }

    if (dependencies.apparentPlaceCalculator == nullptr) {
        std::vector<StarAstrometryBatchResult> results;
        results.reserve(calculatorResults.size());
        for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = missingApparentPlaceCalculatorResult(
                        calculatorResult.result, request.options.correctionFlags()
                    ),
                }
            );
        }
        return results;
    }

    return dependencies.apparentPlaceCalculator->applyBatch(
        request, bodies, calculatorResults, std::move(preparedState)
    );
}

}  // namespace

class HighPrecisionEphemerisEngine::Impl {
public:
    struct DirectBodyStateCacheEntry {
        EphemerisRequest request;
        std::size_t bodyIndex = 0U;
        CelestialBodyState state;
    };

    Impl(
        const CelestialBodyCatalog& catalog,
        EphemerisEngineOptions engineOptions,
        HighPrecisionEphemerisEngineDependencies dependencies
    )
        : m_catalog(std::make_shared<CelestialBodyCatalog>(catalog)),
          m_catalogStarAstrometryArrays(m_catalog->bodies()), m_options(engineOptions),
          m_dependencies(std::move(dependencies))
    {
        m_options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    }

    [[nodiscard]] EphemerisEngineKind::Type kind() const noexcept
    {
        return EphemerisEngineKind::Type::HighPrecision;
    }

    [[nodiscard]] std::string_view name() const noexcept
    {
        return kHighPrecisionEngineName;
    }

    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept
    {
        EphemerisCapabilities caps = EphemerisCapabilities::noCapabilities();
        if (m_dependencies.solarSystemStateCalculator != nullptr) {
            caps |= EphemerisCapabilities::solarSystemBodies();
        }
        if (m_dependencies.starAstrometryCalculator != nullptr) {
            caps |= EphemerisCapabilities::catalogStars();
        }
        if (m_dependencies.earthOrientationProvider != nullptr) {
            caps |= EphemerisCapabilities::topocentricPositions();
        }
        if (m_dependencies.atmosphericRefractionCalculator != nullptr) {
            caps |= EphemerisCapabilities::atmosphericRefraction();
        }
        if (!m_dependencies.dataSetInfo.dateRanges.empty()) {
            caps |= EphemerisCapabilities::extendedHistoricalRange();
        }
        return caps;
    }

    [[nodiscard]] std::span<const EphemerisDateRange> supportedDateRanges() const noexcept
    {
        return m_dependencies.dataSetInfo.dateRanges;
    }

    [[nodiscard]] EphemerisDatasetInfo dataSetInfo() const
    {
        return m_dependencies.dataSetInfo;
    }

    [[nodiscard]] EphemerisEngineOptions options() const noexcept
    {
        return m_options;
    }

    [[nodiscard]] EphemerisRequest
    makeCompatibilityRequest(const skygate::core::ObservationContext& context) const noexcept
    {
        return EphemerisRequestFactory::requestFromContext(context, m_options);
    }

    [[nodiscard]] EphemerisSnapshot compute(const EphemerisRequest& request) const
    {
        if (m_dependencies.computationCache != nullptr) {
            if (std::optional<EphemerisSnapshot> cachedSnapshot = m_dependencies.computationCache->findSnapshot(
                    request, m_catalog->bodies(), m_dependencies.dataSetInfo
                );
                cachedSnapshot.has_value()) {
                return *cachedSnapshot;
            }
        }

        const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = preparedRequestState(request);
        EphemerisSnapshot snapshot = computeUncached(request, preparedState);
        if (m_dependencies.computationCache != nullptr) {
            m_dependencies.computationCache->storeSnapshot(
                request, m_catalog->bodies(), m_dependencies.dataSetInfo, snapshot
            );
        }
        return snapshot;
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
    {
        if (bodyId.empty()) {
            return std::nullopt;
        }

        for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
            const BaseCelestialBody& body = m_catalog->bodyAt(bodyIndex);
            if (StringUtilities::equalsIgnoreAsciiCase(body.id, bodyId)) {
                if (isSolarSystemBody(body)) {
                    if (std::optional<CelestialBodyState> cachedState = findDirectBodyState(request, bodyIndex);
                        cachedState.has_value()) {
                        return cachedState;
                    }
                    CelestialBodyState state =
                        computeStateForBody(request, bodyIndex, buildPreparedRequestState(request));
                    storeDirectBodyState(request, bodyIndex, state);
                    return state;
                }

                if (m_dependencies.computationCache != nullptr) {
                    if (std::optional<EphemerisSnapshot> cachedSnapshot = m_dependencies.computationCache->findSnapshot(
                            request, m_catalog->bodies(), m_dependencies.dataSetInfo
                        );
                        cachedSnapshot.has_value() && bodyIndex < cachedSnapshot->states.size()) {
                        return cachedSnapshot->states[bodyIndex];
                    }
                    if (std::optional<CelestialBodyState> cachedBodyState =
                            m_dependencies.computationCache->findBodyState(
                                request, m_catalog->bodies(), m_dependencies.dataSetInfo, bodyIndex
                            );
                        cachedBodyState.has_value()) {
                        return cachedBodyState;
                    }
                }
                CelestialBodyState state = computeStateForBody(request, bodyIndex, preparedRequestState(request));
                if (m_dependencies.computationCache != nullptr) {
                    m_dependencies.computationCache->storeBodyState(
                        request, m_catalog->bodies(), m_dependencies.dataSetInfo, bodyIndex, state
                    );
                }
                return state;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
    {
        if (bodyIndex >= m_catalog->size() || bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }

        const BaseCelestialBody& body = m_catalog->bodyAt(bodyIndex);
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
            if (std::optional<EphemerisSnapshot> cachedSnapshot = m_dependencies.computationCache->findSnapshot(
                    request, m_catalog->bodies(), m_dependencies.dataSetInfo
                );
                cachedSnapshot.has_value() && bodyIndex < cachedSnapshot->states.size()) {
                return cachedSnapshot->states[bodyIndex];
            }
            if (std::optional<CelestialBodyState> cachedBodyState = m_dependencies.computationCache->findBodyState(
                    request, m_catalog->bodies(), m_dependencies.dataSetInfo, bodyIndex
                );
                cachedBodyState.has_value()) {
                return cachedBodyState;
            }
        }

        CelestialBodyState state = computeStateForBody(request, bodyIndex, preparedRequestState(request));
        if (m_dependencies.computationCache != nullptr) {
            m_dependencies.computationCache->storeBodyState(
                request, m_catalog->bodies(), m_dependencies.dataSetInfo, bodyIndex, state
            );
        }
        return state;
    }

private:
    [[nodiscard]] EphemerisSnapshot computeUncached(
        const EphemerisRequest& request, std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const
    {
        EphemerisSnapshot snapshot;
        snapshot.context = request.context;
        snapshot.catalogBodies = m_catalog;
        snapshot.states.resize(m_catalog->size());

        const IEphemerisResultBuilder& builder = resultBuilder(m_dependencies);
        if (!hasValidEpoch(request.epoch)) {
            for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
                const HighPrecisionComputationInput input{
                    .request = request,
                    .body = m_catalog->bodyAt(bodyIndex),
                    .preparedRequestState = preparedState,
                    .bodyIndex = bodyIndex,
                };
                snapshot.states[bodyIndex] = builder.buildFailedState(input);
                if (isSolarSystemBody(m_catalog->bodyAt(bodyIndex))) {
                    storeDirectBodyState(request, bodyIndex, snapshot.states[bodyIndex]);
                }
            }
            return snapshot;
        }

        std::vector<std::uint8_t> batchFilledStates(m_catalog->size(), 0U);
        const IStarAstrometryCalculator* starAstrometryCalculator = m_dependencies.starAstrometryCalculator.get();
        if (starAstrometryCalculator != nullptr && !m_catalogStarAstrometryArrays.empty()) {
            const std::vector<StarAstrometryBatchResult> batchResults =
                starAstrometryCalculator->calculateBatch(request, m_catalogStarAstrometryArrays, preparedState);
            const std::vector<StarAstrometryBatchResult> apparentBatchResults = applyApparentPlaceBatchIfRequested(
                m_dependencies, request, m_catalog->bodies(), batchResults, preparedState
            );
            for (const StarAstrometryBatchResult& batchResult : apparentBatchResults) {
                if (batchResult.bodyIndex >= m_catalog->size()
                    || batchResult.bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
                    continue;
                }

                const HighPrecisionComputationInput input{
                    .request = request,
                    .body = m_catalog->bodyAt(batchResult.bodyIndex),
                    .preparedRequestState = preparedState,
                    .bodyIndex = batchResult.bodyIndex,
                };
                snapshot.states[batchResult.bodyIndex] = builder.buildState(input, batchResult.result);
                batchFilledStates[batchResult.bodyIndex] = 1U;
            }
        }

        for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
            if (batchFilledStates[bodyIndex] != 0U) {
                continue;
            }
            snapshot.states[bodyIndex] = computeStateForBody(request, bodyIndex, preparedState);
            if (isSolarSystemBody(m_catalog->bodyAt(bodyIndex))) {
                storeDirectBodyState(request, bodyIndex, snapshot.states[bodyIndex]);
            }
        }

        return snapshot;
    }

    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState>
    preparedRequestState(const EphemerisRequest& request) const
    {
        if (m_dependencies.computationCache != nullptr) {
            if (std::shared_ptr<const PreparedEphemerisRequestState> cachedState =
                    m_dependencies.computationCache->findPreparedRequestState(
                        request, m_catalog->bodies(), m_dependencies.dataSetInfo
                    );
                cachedState != nullptr) {
                return cachedState;
            }
        }

        std::shared_ptr<const PreparedEphemerisRequestState> preparedState = buildPreparedRequestState(request);
        if (m_dependencies.computationCache != nullptr) {
            m_dependencies.computationCache->storePreparedRequestState(
                request, m_catalog->bodies(), m_dependencies.dataSetInfo, preparedState
            );
        }
        return preparedState;
    }

    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState>
    buildPreparedRequestState(const EphemerisRequest& request) const
    {
        auto preparedState = std::make_shared<PreparedEphemerisRequestState>();
        {
            if (request.epoch.timeScale == TimeScale::Tdb) {
                preparedState->tdbKernelEpoch = request.epoch.normalized();
            } else if (m_dependencies.timeScaleService == nullptr) {
                preparedState->tdbKernelEpochMetadata.status = EphemerisEngineQueryStatus::Type::Degraded;
                preparedState->tdbKernelEpochMetadata.addWarning(
                    EphemerisEngineWarning::Code::TimeScaleDataUnavailable
                );
            } else {
                const TimeScaleConversionResult conversion =
                    m_dependencies.timeScaleService->convert(request.epoch, TimeScale::Tdb);
                mergeKernelEpochTimeScaleMetadata(preparedState->tdbKernelEpochMetadata, conversion);
                if (conversion.isSuccess()) {
                    preparedState->tdbKernelEpoch = conversion.epoch.normalized();
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
                    preparedState->topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
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
                        preparedState->topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
                    );
                }
            }
        }

        return preparedState;
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    findDirectBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
    {
        const std::scoped_lock lock(m_directBodyStateCacheMutex);
        for (auto cacheEntry = m_directBodyStateCache.rbegin(); cacheEntry != m_directBodyStateCache.rend();
             ++cacheEntry) {
            if (cacheEntry->bodyIndex == bodyIndex && sameRequest(cacheEntry->request, request)) {
                return cacheEntry->state;
            }
        }

        return std::nullopt;
    }

    void storeDirectBodyState(
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

    [[nodiscard]] CelestialBodyState computeStateForBody(
        const EphemerisRequest& request,
        const std::size_t bodyIndex,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const
    {
        const BaseCelestialBody& body = m_catalog->bodyAt(bodyIndex);
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

    std::shared_ptr<const CelestialBodyCatalog> m_catalog;
    CatalogStarAstrometryArrays m_catalogStarAstrometryArrays;
    EphemerisEngineOptions m_options;
    HighPrecisionEphemerisEngineDependencies m_dependencies;
    mutable std::mutex m_directBodyStateCacheMutex;
    mutable std::deque<DirectBodyStateCacheEntry> m_directBodyStateCache;
};

// Public interface delegates
HighPrecisionEphemerisEngine::HighPrecisionEphemerisEngine(
    const CelestialBodyCatalog& catalog,
    EphemerisEngineOptions engineOptions,
    HighPrecisionEphemerisEngineDependencies dependencies
)
    : m_impl(std::make_unique<Impl>(catalog, engineOptions, std::move(dependencies)))
{
}

HighPrecisionEphemerisEngine::~HighPrecisionEphemerisEngine() = default;

HighPrecisionEphemerisEngine::HighPrecisionEphemerisEngine(HighPrecisionEphemerisEngine&&) noexcept = default;
HighPrecisionEphemerisEngine&
HighPrecisionEphemerisEngine::operator=(HighPrecisionEphemerisEngine&&) noexcept = default;

EphemerisEngineKind::Type HighPrecisionEphemerisEngine::kind() const noexcept
{
    return m_impl->kind();
}

std::string_view HighPrecisionEphemerisEngine::name() const noexcept
{
    return m_impl->name();
}

EphemerisCapabilities HighPrecisionEphemerisEngine::capabilities() const noexcept
{
    return m_impl->capabilities();
}

std::span<const EphemerisDateRange> HighPrecisionEphemerisEngine::supportedDateRanges() const noexcept
{
    return m_impl->supportedDateRanges();
}

EphemerisDatasetInfo HighPrecisionEphemerisEngine::dataSetInfo() const
{
    return m_impl->dataSetInfo();
}

EphemerisEngineOptions HighPrecisionEphemerisEngine::options() const noexcept
{
    return m_impl->options();
}

EphemerisSnapshot HighPrecisionEphemerisEngine::compute(const EphemerisRequest& request) const
{
    return m_impl->compute(request);
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
{
    return m_impl->computeBodyState(request, bodyId);
}

std::optional<CelestialBodyState>
HighPrecisionEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    return m_impl->computeBodyState(request, bodyIndex);
}

EphemerisSnapshot HighPrecisionEphemerisEngine::compute(const skygate::core::ObservationContext& context) const
{
    return m_impl->compute(m_impl->makeCompatibilityRequest(context));
}

std::optional<CelestialBodyState> HighPrecisionEphemerisEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::string_view bodyId
) const
{
    return m_impl->computeBodyState(m_impl->makeCompatibilityRequest(context), bodyId);
}

std::optional<CelestialBodyState> HighPrecisionEphemerisEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::uint32_t bodyIndex
) const
{
    return m_impl->computeBodyState(m_impl->makeCompatibilityRequest(context), static_cast<std::size_t>(bodyIndex));
}

}  // namespace skygate::ephemeris::highprecision
