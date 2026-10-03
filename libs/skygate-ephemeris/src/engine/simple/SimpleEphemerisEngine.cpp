#include "SimpleEphemerisEngine.hpp"
#include "EphemerisRequestFactory.hpp"
#include "EquatorialToHorizontalCalculator.hpp"
#include "StringUtilities.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {
namespace {

constexpr std::string_view kSimpleDataSourceProvenance = "Simple ephemeris engine";
constexpr std::string_view kSimpleEngineName = "Simple ephemeris engine";
constexpr std::string_view kSimpleDataSetId = "simple";
constexpr std::string_view kSimpleDataSetVersion = "built-in";

[[nodiscard]] EphemerisCorrectionFlags requestedCorrectionTerms(const EphemerisEngineOptions& options) noexcept
{
    return options.correctionFlags().without(EphemerisCorrectionFlags::geometric());
}

void markUnsupportedSimpleOptions(CelestialBodyState& state, const EphemerisEngineOptions& options) noexcept
{
    const EphemerisCorrectionFlags requested = options.correctionFlags();
    const EphemerisCorrectionFlags correctionTerms = requestedCorrectionTerms(options);
    if (!correctionTerms.hasCorrections()) {
        state.metadata.appliedCorrections = requested == EphemerisCorrectionFlags::geometric()
                                                ? EphemerisCorrectionFlags::geometric()
                                                : EphemerisCorrectionFlags::noCorrections();
        state.metadata.finalizeCorrectionTracking(requested);
        return;
    }

    if (state.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
        state.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
    state.metadata.appliedCorrections = requested.has(EphemerisCorrectionFlags::geometric())
                                            ? EphemerisCorrectionFlags::geometric()
                                            : EphemerisCorrectionFlags::noCorrections();
    state.metadata.addUnavailableCorrection(correctionTerms);
    state.metadata.finalizeCorrectionTracking(requested);
}

void markUnsupportedSimpleOptions(EphemerisSnapshot& snapshot, const EphemerisEngineOptions& options) noexcept
{
    for (CelestialBodyState& state : snapshot.states) {
        markUnsupportedSimpleOptions(state, options);
    }
}

[[nodiscard]] EphemerisEngineOptions
simpleEngineOptionsFromRequest(const EphemerisEngineOptions& requestOptions) noexcept
{
    EphemerisEngineOptions engineOptions = requestOptions;
    engineOptions.setEngineKind(EphemerisEngineKind::Type::Simple);
    return engineOptions;
}

[[nodiscard]] bool hasUnsupportedExplicitEpoch(const EphemerisRequest& request) noexcept
{
    return request.epoch.hasExplicit() && request.epoch.timeScale != skygate::core::TimeScale::Utc;
}

[[nodiscard]] CelestialBodyState makeUnsupportedEpochState(const std::size_t bodyIndex) noexcept
{
    CelestialBodyState state;
    state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
    state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
    state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
    state.metadata.status = EphemerisEngineQueryStatus::Type::Unsupported;
    state.metadata.addWarning(EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion);
    state.metadata.dataSourceProvenance = kSimpleDataSourceProvenance;
    return state;
}

}  // namespace

SimpleEphemerisEngine::SimpleEphemerisEngine(const CelestialBodyCatalog& catalog, EphemerisEngineOptions engineOptions)
    : m_catalog(std::make_shared<CelestialBodyCatalog>(catalog)),
      m_options(simpleEngineOptionsFromRequest(engineOptions))
{
}

EphemerisEngineKind::Type SimpleEphemerisEngine::kind() const noexcept
{
    return EphemerisEngineKind::Type::Simple;
}

std::string_view SimpleEphemerisEngine::name() const noexcept
{
    return kSimpleEngineName;
}

EphemerisCapabilities SimpleEphemerisEngine::capabilities() const noexcept
{
    return EphemerisCapabilities::simpleEngine();
}

std::span<const EphemerisDateRange> SimpleEphemerisEngine::supportedDateRanges() const noexcept
{
    return {};
}

EphemerisDatasetInfo SimpleEphemerisEngine::dataSetInfo() const
{
    EphemerisDatasetInfo info;
    info.id = kSimpleDataSetId;
    info.displayName = kSimpleEngineName;
    info.version = kSimpleDataSetVersion;
    info.provenance = kSimpleDataSourceProvenance;
    return info;
}

EphemerisEngineOptions SimpleEphemerisEngine::options() const noexcept
{
    return m_options;
}

EphemerisSnapshot SimpleEphemerisEngine::compute(const EphemerisRequest& request) const
{
    if (hasUnsupportedExplicitEpoch(request)) {
        EphemerisSnapshot snapshot;
        snapshot.context = request.context;
        snapshot.catalogBodies = m_catalog;
        snapshot.states.reserve(m_catalog->size());
        for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
            snapshot.states.push_back(makeUnsupportedEpochState(bodyIndex));
        }
        return snapshot;
    }

    EphemerisSnapshot snapshot = computeSnapshot(EphemerisRequestFactory::contextFromRequest(request));
    markUnsupportedSimpleOptions(snapshot, request.options);
    return snapshot;
}

std::optional<CelestialBodyState>
SimpleEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
{
    if (hasUnsupportedExplicitEpoch(request)) {
        for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
            const BaseCelestialBody& body = m_catalog->bodyAt(bodyIndex);
            if (StringUtilities::equalsIgnoreAsciiCase(body.id, bodyId)) {
                return makeUnsupportedEpochState(bodyIndex);
            }
        }
        return std::nullopt;
    }

    std::optional<CelestialBodyState> state =
        computeBodyStateById(EphemerisRequestFactory::contextFromRequest(request), bodyId);
    if (state.has_value()) {
        markUnsupportedSimpleOptions(*state, request.options);
    }
    return state;
}

std::optional<CelestialBodyState>
SimpleEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    if (bodyIndex >= m_catalog->size()) {
        return std::nullopt;
    }

    if (hasUnsupportedExplicitEpoch(request)) {
        return makeUnsupportedEpochState(bodyIndex);
    }

    CelestialBodyState state = computeStateForBody(
        m_catalog->bodyAt(bodyIndex), bodyIndex, EphemerisRequestFactory::contextFromRequest(request)
    );
    markUnsupportedSimpleOptions(state, request.options);
    return state;
}

EphemerisSnapshot SimpleEphemerisEngine::compute(const skygate::core::ObservationContext& context) const
{
    EphemerisSnapshot snapshot = computeSnapshot(context);
    markUnsupportedSimpleOptions(snapshot, options());
    return snapshot;
}

std::optional<CelestialBodyState> SimpleEphemerisEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::string_view bodyId
) const
{
    std::optional<CelestialBodyState> state = computeBodyStateById(context, bodyId);
    if (state.has_value()) {
        markUnsupportedSimpleOptions(*state, options());
    }
    return state;
}

std::optional<CelestialBodyState> SimpleEphemerisEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::uint32_t bodyIndex
) const
{
    if (bodyIndex >= m_catalog->size()) {
        return std::nullopt;
    }

    CelestialBodyState state = computeStateForBody(m_catalog->bodyAt(bodyIndex), bodyIndex, context);
    markUnsupportedSimpleOptions(state, options());
    return state;
}

EphemerisSnapshot SimpleEphemerisEngine::computeSnapshot(const skygate::core::ObservationContext& context) const
{
    EphemerisSnapshot snapshot;
    snapshot.context = context;
    snapshot.catalogBodies = m_catalog;

    snapshot.states.reserve(m_catalog->size());
    for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
        const BaseCelestialBody& body = m_catalog->bodyAt(bodyIndex);
        snapshot.states.push_back(computeStateForBody(body, bodyIndex, context));
    }

    return snapshot;
}

std::optional<CelestialBodyState> SimpleEphemerisEngine::computeBodyStateById(
    const skygate::core::ObservationContext& context, const std::string_view bodyId
) const
{
    if (bodyId.empty()) {
        return std::nullopt;
    }

    for (std::size_t bodyIndex = 0; bodyIndex < m_catalog->size(); ++bodyIndex) {
        const BaseCelestialBody& body = m_catalog->bodyAt(bodyIndex);
        if (StringUtilities::equalsIgnoreAsciiCase(body.id, bodyId)) {
            return computeStateForBody(body, bodyIndex, context);
        }
    }

    return std::nullopt;
}

CelestialBodyState SimpleEphemerisEngine::computeStateForBody(
    const BaseCelestialBody& body, const std::size_t bodyIndex, const skygate::core::ObservationContext& context
) const
{
    CelestialBodyState state;
    state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
    state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
    state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
    state.metadata.dataSourceProvenance = kSimpleDataSourceProvenance;

    if (const auto equatorial = computeEquatorial(body, context.utcTime); equatorial.has_value()) {
        state.equatorial = *equatorial;
        if (context.observer.isValid()) {
            state.horizontal =
                EquatorialToHorizontalCalculator::compute(*equatorial, context.observer, context.utcTime);
        } else {
            state.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
            state.metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
        }
    } else {
        state.metadata.status = EphemerisEngineQueryStatus::Type::Unsupported;
        state.metadata.addWarning(EphemerisEngineWarning::Code::UnsupportedBody);
    }

    return state;
}

std::optional<skygate::core::EquatorialCoordinate> SimpleEphemerisEngine::computeEquatorial(
    const BaseCelestialBody& body, const skygate::core::UtcTimePoint& utcTime
) const
{
    if (body.fixedEquatorialValue().has_value()) {
        return body.fixedEquatorialValue();
    }

    switch (body.kind) {
    case BaseCelestialBody::Kind::Sun:
        return m_sunCalculator.compute(utcTime);
    case BaseCelestialBody::Kind::Moon:
        return m_moonCalculator.compute(utcTime);
    case BaseCelestialBody::Kind::Planet:
        return m_planetCalculator.compute(body.id, utcTime);
    case BaseCelestialBody::Kind::Star:
    case BaseCelestialBody::Kind::Constellation:
    case BaseCelestialBody::Kind::DeepSkyObject:
        break;
    }

    return std::nullopt;
}

EphemerisEngineOptions SimpleEphemerisEngine::defaultOptions() noexcept
{
    EphemerisEngineOptions engineOptions;
    engineOptions.setEngineKind(EphemerisEngineKind::Type::Simple);
    engineOptions.setCorrectionFlags(EphemerisCorrectionFlags::noCorrections());
    engineOptions.setEnableAtmosphericRefraction(false);
    return engineOptions;
}

}  // namespace skygate::ephemeris
