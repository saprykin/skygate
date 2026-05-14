#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include "StringUtilities.hpp"
#include "engine/highprecision/EphemerisResultBuilder.hpp"

#include <chrono>
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
constexpr double kSecondsPerDay = 86'400.0;
constexpr double kUnixEpochJulianDay = 2'440'587.5;

[[nodiscard]] bool hasValidEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay =
        static_cast<double>(utcTime.time_since_epoch().count()) / kSecondsPerDay + kUnixEpochJulianDay;
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
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

[[nodiscard]] std::vector<StarAstrometryBatchResult> applyApparentPlaceBatchIfRequested(
    const HighPrecisionEphemerisEngineDependencies& dependencies,
    const EphemerisRequest& request,
    const std::span<const CelestialBody> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults
)
{
    if (!requestsApparentPlaceProcessing(request)) {
        return {calculatorResults.begin(), calculatorResults.end()};
    }

    return apparentPlaceCalculator(dependencies).applyBatch(request, bodies, calculatorResults);
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

    SkySnapshot snapshot = computeUncached(request);
    if (m_dependencies.computationCache != nullptr) {
        m_dependencies.computationCache->storeSnapshot(request, *m_bodies, m_dependencies.dataSetInfo, snapshot);
    }
    return snapshot;
}

SkySnapshot HighPrecisionEphemerisEngine::computeUncached(const EphemerisRequest& request) const
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
            starAstrometryCalculator->calculateBatch(request, m_catalogStarAstrometryArrays);
        const std::vector<StarAstrometryBatchResult> apparentBatchResults =
            applyApparentPlaceBatchIfRequested(m_dependencies, request, *m_bodies, batchResults);
        for (const StarAstrometryBatchResult& batchResult : apparentBatchResults) {
            if (batchResult.bodyIndex >= m_bodies->size()
                || batchResult.bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
                continue;
            }

            const HighPrecisionComputationInput input{
                .request = request,
                .body = (*m_bodies)[batchResult.bodyIndex],
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
        snapshot.states[bodyIndex] = computeStateForBody(request, bodyIndex);
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
            return computeStateForBody(request, bodyIndex);
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
    }

    return computeStateForBody(request, bodyIndex);
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
    EphemerisRequest request;
    request.epoch = epochFromUtcTime(context.utcTime);
    request.context = context;
    request.options = m_options;
    return request;
}

CelestialBodyState
HighPrecisionEphemerisEngine::computeStateForBody(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    const CelestialBody& body = (*m_bodies)[bodyIndex];
    const HighPrecisionComputationInput input{
        .request = request,
        .body = body,
        .bodyIndex = bodyIndex,
    };
    const IEphemerisResultBuilder& builder = resultBuilder(m_dependencies);

    if (!hasValidEpoch(request.epoch)) {
        return builder.buildFailedState(input);
    }

    const ISolarSystemStateCalculator* solarSystemCalculator = m_dependencies.solarSystemStateCalculator.get();
    if (isSolarSystemBody(body) && solarSystemCalculator != nullptr) {
        HighPrecisionCalculatorResult calculatorResult = solarSystemCalculator->calculate(input);
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
