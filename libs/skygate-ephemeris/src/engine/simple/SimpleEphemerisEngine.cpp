#include "engine/simple/SimpleEphemerisEngine.hpp"

#include "StringUtilities.hpp"
#include "engine/simple/EquatorialToHorizontalCalculator.hpp"
#include "skygate/ephemeris/EphemerisRequestFactory.hpp"

#include <cmath>
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

[[nodiscard]] bool requestsUnsupportedSimpleOptions(const EphemerisEngineOptions& options) noexcept
{
    return options.correctionFlags != EphemerisCorrectionFlags::NoCorrections;
}

void markUnsupportedSimpleOptions(CelestialBodyState& state, const EphemerisEngineOptions& options) noexcept
{
    state.metadata.finalizeCorrectionTracking(options.correctionFlags);
    if (!requestsUnsupportedSimpleOptions(options)) {
        return;
    }

    if (state.metadata.status == EphemerisResultStatus::Valid) {
        state.metadata.status = EphemerisResultStatus::Degraded;
    }
    state.metadata.appliedCorrections = EphemerisCorrectionFlags::NoCorrections;
    state.metadata.addUnavailableCorrection(options.correctionFlags);
    state.metadata.finalizeCorrectionTracking(options.correctionFlags);
}

void markUnsupportedSimpleOptions(SkySnapshot& snapshot, const EphemerisEngineOptions& options) noexcept
{
    if (!requestsUnsupportedSimpleOptions(options)) {
        return;
    }

    for (CelestialBodyState& state : snapshot.states) {
        markUnsupportedSimpleOptions(state, options);
    }
}

[[nodiscard]] EphemerisEngineOptions
simpleEngineOptionsFromRequest(const EphemerisEngineOptions& requestOptions) noexcept
{
    EphemerisEngineOptions engineOptions = requestOptions;
    engineOptions.engineKind = EphemerisEngineKind::Simple;
    return engineOptions;
}

}  // namespace

SimpleEphemerisEngine::SimpleEphemerisEngine(
    std::span<const CelestialBody> bodies, EphemerisEngineOptions engineOptions
)
    : m_bodies(std::make_shared<const std::vector<CelestialBody>>(bodies.begin(), bodies.end())),
      m_options(simpleEngineOptionsFromRequest(engineOptions))
{
}

EphemerisEngineKind SimpleEphemerisEngine::kind() const noexcept
{
    return EphemerisEngineKind::Simple;
}

std::string_view SimpleEphemerisEngine::name() const noexcept
{
    return kSimpleEngineName;
}

EphemerisCapabilities SimpleEphemerisEngine::capabilities() const noexcept
{
    EphemerisCapabilities engineCapabilities;
    engineCapabilities.engineKind = EphemerisEngineKind::Simple;
    engineCapabilities.supportedCorrections = EphemerisCorrectionFlags::NoCorrections;
    engineCapabilities.supportsSolarSystemBodies = true;
    engineCapabilities.supportsCatalogStars = true;
    engineCapabilities.supportsTopocentricPositions = true;
    engineCapabilities.supportsAtmosphericRefraction = false;
    engineCapabilities.supportsExtendedHistoricalRange = false;
    return engineCapabilities;
}

std::span<const EphemerisDateRange> SimpleEphemerisEngine::supportedDateRanges() const noexcept
{
    return {};
}

EphemerisDataSetInfo SimpleEphemerisEngine::dataSetInfo() const
{
    EphemerisDataSetInfo info;
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

SkySnapshot SimpleEphemerisEngine::compute(const EphemerisRequest& request) const
{
    SkySnapshot snapshot = computeSnapshot(EphemerisRequestFactory::contextFromRequest(request));
    markUnsupportedSimpleOptions(snapshot, request.options);
    return snapshot;
}

std::optional<CelestialBodyState>
SimpleEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
{
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
    if (bodyIndex >= m_bodies->size()) {
        return std::nullopt;
    }

    CelestialBodyState state =
        computeStateForBody((*m_bodies)[bodyIndex], bodyIndex, EphemerisRequestFactory::contextFromRequest(request));
    markUnsupportedSimpleOptions(state, request.options);
    return state;
}

SkySnapshot SimpleEphemerisEngine::compute(const core::SkyContext& context) const
{
    SkySnapshot snapshot = computeSnapshot(context);
    markUnsupportedSimpleOptions(snapshot, options());
    return snapshot;
}

std::optional<CelestialBodyState>
SimpleEphemerisEngine::computeBodyState(const core::SkyContext& context, const std::string_view bodyId) const
{
    std::optional<CelestialBodyState> state = computeBodyStateById(context, bodyId);
    if (state.has_value()) {
        markUnsupportedSimpleOptions(*state, options());
    }
    return state;
}

std::optional<CelestialBodyState>
SimpleEphemerisEngine::computeBodyState(const core::SkyContext& context, const std::uint32_t bodyIndex) const
{
    if (bodyIndex >= m_bodies->size()) {
        return std::nullopt;
    }

    CelestialBodyState state = computeStateForBody((*m_bodies)[bodyIndex], bodyIndex, context);
    markUnsupportedSimpleOptions(state, options());
    return state;
}

SkySnapshot SimpleEphemerisEngine::computeSnapshot(const core::SkyContext& context) const
{
    SkySnapshot snapshot;
    snapshot.context = context;
    snapshot.catalogBodies = m_bodies;

    snapshot.states.reserve(m_bodies->size());
    for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        const CelestialBody& body = (*m_bodies)[bodyIndex];
        snapshot.states.push_back(computeStateForBody(body, bodyIndex, context));
    }

    return snapshot;
}

std::optional<CelestialBodyState>
SimpleEphemerisEngine::computeBodyStateById(const core::SkyContext& context, const std::string_view bodyId) const
{
    if (bodyId.empty()) {
        return std::nullopt;
    }

    for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        const CelestialBody& body = (*m_bodies)[bodyIndex];
        if (strings::equalsIgnoreAsciiCase(body.id, bodyId)) {
            return computeStateForBody(body, bodyIndex, context);
        }
    }

    return std::nullopt;
}

CelestialBodyState SimpleEphemerisEngine::computeStateForBody(
    const CelestialBody& body, const std::size_t bodyIndex, const core::SkyContext& context
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
            state.metadata.status = EphemerisResultStatus::Degraded;
            state.metadata.addWarning(EphemerisWarningCode::MissingObserver);
        }
    } else {
        state.metadata.status = EphemerisResultStatus::Unsupported;
        state.metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
    }

    return state;
}

std::optional<core::EquatorialCoordinate>
SimpleEphemerisEngine::computeEquatorial(const CelestialBody& body, const core::UtcTimePoint& utcTime) const
{
    if (body.fixedEquatorial.has_value()) {
        return body.fixedEquatorial;
    }

    switch (body.ephemerisSource) {
    case CelestialBodyEphemerisSource::FixedEquatorial:
        break;
    case CelestialBodyEphemerisSource::Sun:
        return m_sunCalculator.compute(utcTime);
    case CelestialBodyEphemerisSource::Moon:
        return m_moonCalculator.compute(utcTime);
    case CelestialBodyEphemerisSource::Planet:
        return m_planetCalculator.compute(body.id, utcTime);
    case CelestialBodyEphemerisSource::Star:
        break;
    case CelestialBodyEphemerisSource::Constellation:
        break;
    case CelestialBodyEphemerisSource::Unresolved:
        break;
    }

    return std::nullopt;
}

EphemerisEngineOptions simpleEphemerisEngineDefaultOptions() noexcept
{
    EphemerisEngineOptions engineOptions;
    engineOptions.engineKind = EphemerisEngineKind::Simple;
    engineOptions.correctionFlags = EphemerisCorrectionFlags::NoCorrections;
    engineOptions.enableAtmosphericRefraction = false;
    return engineOptions;
}

}  // namespace skygate::ephemeris
