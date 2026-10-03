#include "HighPrecisionEphemerisEngine.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "EphemerisRequestFactory.hpp"
#include "EphemerisResultBuilder.hpp"
#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "IApparentPlaceCalculator.hpp"
#include "IEphemerisComputationCache.hpp"
#include "IEphemerisResultBuilder.hpp"
#include "ISolarSystemStateCalculator.hpp"
#include "IStarAstrometryCalculator.hpp"
#include "engine/ITimeScaleService.hpp"
#include "PreparedEphemerisRequestState.hpp"
#include "PreparedRequestStateBuilder.hpp"
#include "StarAstrometryBatchResult.hpp"
#include "StringUtilities.hpp"
#include "UtcTimeCodec.hpp"
#include "engine/IEphemerisFallbackStrategy.hpp"

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

// The request epoch is the single authoritative computation instant. A
// non-explicit (default/zero) epoch is rejected instead of being derived from
// context.utcTime. Non-UTC explicit epochs are honored: TDB is used directly
// and other scales are converted by the injected time-scale service.
[[nodiscard]] bool hasValidEpoch(const skygate::core::AstronomicalEpoch& epoch) noexcept
{
    return epoch.hasExplicit();
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
    return request.options.correctionFlags().hasCorrections();
}

[[nodiscard]] std::optional<CelestialBodyState> computeSimpleFallbackState(
    const HighPrecisionEphemerisEngine::Dependencies& dependencies,
    const HighPrecisionComputationInput& input,
    const HighPrecisionCalculatorResult& originalResult
)
{
    if (!input.request.options.fallbackToSimpleEngine() || originalResult.equatorial.has_value()
        || !originalResult.metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange)
        || dependencies.fallbackStrategy == nullptr) {
        return std::nullopt;
    }

    std::optional<CelestialBodyState> fallbackState =
        dependencies.fallbackStrategy->computeFallbackState(input.request, input.body, input.bodyIndex);
    if (!fallbackState.has_value()) {
        return std::nullopt;
    }

    fallbackState->metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    fallbackState->metadata.warningCodeMask |= originalResult.metadata.warningCodeMask;
    fallbackState->metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    fallbackState->metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    return fallbackState;
}

void mergeKernelEpochTimeScaleMetadata(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    constexpr std::uint32_t kTdbApproximationWarning =
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::TdbApproximationApplied);
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

[[nodiscard]] const IEphemerisResultBuilder&
resultBuilder(const HighPrecisionEphemerisEngine::Dependencies& dependencies)
{
    static const EphemerisResultBuilder kDefaultBuilder;
    if (dependencies.resultBuilder != nullptr) {
        return *dependencies.resultBuilder;
    }

    return kDefaultBuilder;
}

[[nodiscard]] HighPrecisionCalculatorResult applyApparentPlaceIfRequested(
    const HighPrecisionEphemerisEngine::Dependencies& dependencies,
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

[[nodiscard]] std::optional<skygate::core::AstronomicalEpoch> kernelEpochForSolarSystemState(
    EphemerisEngineQueryResult& metadata,
    const EphemerisRequest& request,
    const PreparedEphemerisRequestState* preparedState,
    const std::shared_ptr<const ITimeScaleService>& timeScaleService
)
{
    if (request.epoch.timeScale == skygate::core::TimeScale::Tdb) {
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

    const TimeScaleConversionResult conversion =
        timeScaleService->convert(request.epoch, skygate::core::TimeScale::Tdb);
    mergeKernelEpochTimeScaleMetadata(metadata, conversion);
    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return conversion.epoch.normalized();
}

[[nodiscard]] std::vector<StarAstrometryBatchResult> applyApparentPlaceBatchIfRequested(
    const HighPrecisionEphemerisEngine::Dependencies& dependencies,
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

[[nodiscard]] PreparedRequestStateBuilder::Dependencies
makePreparedRequestStateBuilderDependencies(const HighPrecisionEphemerisEngine::Dependencies& dependencies)
{
    return PreparedRequestStateBuilder::Dependencies{
        .calcephKernel = dependencies.calcephKernel,
        .timeScaleService = dependencies.timeScaleService,
        .earthOrientationProvider = dependencies.earthOrientationProvider,
    };
}

}  // namespace

class HighPrecisionEphemerisEngine::Impl {
public:
    Impl(
        const CelestialBodyCatalog& catalog,
        EphemerisEngineOptions engineOptions,
        HighPrecisionEphemerisEngine::Dependencies dependencies
    )
        : m_catalog(std::make_shared<CelestialBodyCatalog>(catalog)),
          m_catalogStarAstrometryArrays(m_catalog->bodies()), m_options(engineOptions),
          m_dependencies(std::move(dependencies)),
          m_preparedRequestStateBuilder(makePreparedRequestStateBuilderDependencies(m_dependencies))
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

    [[nodiscard]] EphemerisEngineTraits traits() const noexcept
    {
        return EphemerisEngineTraits::highPrecisionEngine();
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
                    request, catalogIdentity(), m_dependencies.dataSetInfo
                );
                cachedSnapshot.has_value()) {
                return *cachedSnapshot;
            }
        }

        const std::shared_ptr<const PreparedEphemerisRequestState> preparedState = preparedRequestState(request);
        EphemerisSnapshot snapshot = computeUncached(request, preparedState);
        if (m_dependencies.computationCache != nullptr) {
            m_dependencies.computationCache->storeSnapshot(
                request, catalogIdentity(), m_dependencies.dataSetInfo, snapshot
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
                return computeResolvedBodyState(request, bodyIndex);
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

        return computeResolvedBodyState(request, bodyIndex);
    }

private:
    [[nodiscard]] const CelestialBodyCatalog* catalogIdentity() const noexcept
    {
        return m_catalog.get();
    }

    [[nodiscard]] CelestialBodyState
    computeResolvedBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
    {
        if (m_dependencies.computationCache != nullptr) {
            if (std::optional<CelestialBodyState> cachedSnapshotState =
                    m_dependencies.computationCache->findSnapshotBodyState(
                        request, catalogIdentity(), m_dependencies.dataSetInfo, bodyIndex
                    );
                cachedSnapshotState.has_value()) {
                return *cachedSnapshotState;
            }
            if (std::optional<CelestialBodyState> cachedBodyState = m_dependencies.computationCache->findBodyState(
                    request, catalogIdentity(), m_dependencies.dataSetInfo, bodyIndex
                );
                cachedBodyState.has_value()) {
                return *cachedBodyState;
            }
        }

        CelestialBodyState state = computeStateForBody(request, bodyIndex, preparedRequestState(request));
        if (m_dependencies.computationCache != nullptr) {
            m_dependencies.computationCache->storeBodyState(
                request, catalogIdentity(), m_dependencies.dataSetInfo, bodyIndex, state
            );
        }
        return state;
    }

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
        }

        return snapshot;
    }

    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState>
    preparedRequestState(const EphemerisRequest& request) const
    {
        if (m_dependencies.computationCache != nullptr) {
            if (std::shared_ptr<const PreparedEphemerisRequestState> cachedState =
                    m_dependencies.computationCache->findPreparedRequestState(
                        request, catalogIdentity(), m_dependencies.dataSetInfo
                    );
                cachedState != nullptr) {
                return cachedState;
            }
        }

        std::shared_ptr<const PreparedEphemerisRequestState> preparedState =
            m_preparedRequestStateBuilder.build(request);
        if (m_dependencies.computationCache != nullptr) {
            m_dependencies.computationCache->storePreparedRequestState(
                request, catalogIdentity(), m_dependencies.dataSetInfo, preparedState
            );
        }
        return preparedState;
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
            const std::optional<skygate::core::AstronomicalEpoch> kernelEpoch = kernelEpochForSolarSystemState(
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
            if (std::optional<CelestialBodyState> fallbackState =
                    computeSimpleFallbackState(m_dependencies, input, calculatorResult);
                fallbackState.has_value()) {
                return *fallbackState;
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
    HighPrecisionEphemerisEngine::Dependencies m_dependencies;
    PreparedRequestStateBuilder m_preparedRequestStateBuilder;
};

// Public interface delegates
HighPrecisionEphemerisEngine::HighPrecisionEphemerisEngine(
    const CelestialBodyCatalog& catalog,
    EphemerisEngineOptions engineOptions,
    HighPrecisionEphemerisEngine::Dependencies dependencies
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

EphemerisEngineTraits HighPrecisionEphemerisEngine::traits() const noexcept
{
    return m_impl->traits();
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
    const skygate::core::ObservationContext& context, const std::size_t bodyIndex
) const
{
    return m_impl->computeBodyState(m_impl->makeCompatibilityRequest(context), bodyIndex);
}

}  // namespace skygate::ephemeris::highprecision
