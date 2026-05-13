#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include "StringUtilities.hpp"
#include "engine/simple/EquatorialToHorizontalCalculator.hpp"
#include "engine/simple/MoonEquatorialCalculator.hpp"
#include "engine/simple/PlanetEquatorialCalculator.hpp"
#include "engine/simple/SunEquatorialCalculator.hpp"

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

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kSimpleDataSourceProvenance = "Simple ephemeris engine";
constexpr std::string_view kSimpleEngineName = "Simple ephemeris engine";
constexpr std::string_view kSimpleDataSetId = "simple";
constexpr std::string_view kSimpleDataSetVersion = "built-in";
constexpr double kSecondsPerDay = 86'400.0;
constexpr double kUnixEpochJulianDay = 2'440'587.5;

[[nodiscard]] bool hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2)
           && (epoch.julianDatePart1 != 0.0 || epoch.julianDatePart2 != 0.0);
}

[[nodiscard]] core::UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    const double julianDay = epoch.julianDatePart1 + epoch.julianDatePart2;
    const double epochSeconds = std::round((julianDay - kUnixEpochJulianDay) * kSecondsPerDay);
    return core::UtcTimePoint(std::chrono::seconds(static_cast<std::int64_t>(epochSeconds)));
}

[[nodiscard]] core::SkyContext contextFromRequest(const EphemerisRequest& request) noexcept
{
    core::SkyContext context = request.context;
    if (request.epoch.timeScale == TimeScale::Utc && hasExplicitEpoch(request.epoch)) {
        context.utcTime = utcTimeFromEpoch(request.epoch);
    }

    return context;
}

[[nodiscard]] bool requestsUnsupportedSimpleOptions(const EphemerisEngineOptions& options) noexcept
{
    return options.correctionFlags != EphemerisCorrectionFlags::NoCorrections || options.enableAtmosphericRefraction;
}

void markUnsupportedSimpleOptions(SkySnapshot& snapshot, const EphemerisEngineOptions& options) noexcept
{
    if (!requestsUnsupportedSimpleOptions(options)) {
        return;
    }

    for (CelestialBodyState& state : snapshot.states) {
        if (state.metadata.status == EphemerisResultStatus::Valid) {
            state.metadata.status = EphemerisResultStatus::Degraded;
        }
        state.metadata.addWarning(EphemerisWarningCode::CorrectionUnavailable);
        state.metadata.appliedCorrections = EphemerisCorrectionFlags::NoCorrections;
    }
}

}  // namespace

class SimpleEphemerisEngine final : public IEphemerisEngine {
public:
    explicit SimpleEphemerisEngine(std::span<const CelestialBody> bodies)
        : m_bodies(std::make_shared<const std::vector<CelestialBody>>(bodies.begin(), bodies.end()))
    {
    }

    [[nodiscard]] EphemerisEngineKind kind() const noexcept override
    {
        return EphemerisEngineKind::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return kSimpleEngineName;
    }

    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept override
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

    [[nodiscard]] std::span<const EphemerisDateRange> supportedDateRanges() const noexcept override
    {
        return {};
    }

    [[nodiscard]] EphemerisDataSetInfo dataSetInfo() const override
    {
        EphemerisDataSetInfo info;
        info.id = kSimpleDataSetId;
        info.displayName = kSimpleEngineName;
        info.version = kSimpleDataSetVersion;
        info.provenance = kSimpleDataSourceProvenance;
        return info;
    }

    [[nodiscard]] EphemerisEngineOptions options() const noexcept override
    {
        EphemerisEngineOptions engineOptions;
        engineOptions.engineKind = EphemerisEngineKind::Simple;
        engineOptions.correctionFlags = EphemerisCorrectionFlags::NoCorrections;
        engineOptions.enableAtmosphericRefraction = false;
        return engineOptions;
    }

    [[nodiscard]] SkySnapshot compute(const EphemerisRequest& request) const override
    {
        SkySnapshot snapshot = compute(contextFromRequest(request));
        markUnsupportedSimpleOptions(snapshot, request.options);
        return snapshot;
    }

    [[nodiscard]] SkySnapshot compute(const core::SkyContext& context) const override
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

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, const std::string_view bodyId) const override
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

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }

        return computeStateForBody((*m_bodies)[bodyIndex], bodyIndex, context);
    }

private:
    [[nodiscard]] CelestialBodyState
    computeStateForBody(const CelestialBody& body, const std::size_t bodyIndex, const core::SkyContext& context) const
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

    [[nodiscard]] std::optional<core::EquatorialCoordinate>
    computeEquatorial(const CelestialBody& body, const core::UtcTimePoint& utcTime) const
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

    std::shared_ptr<const std::vector<CelestialBody>> m_bodies;
    SunEquatorialCalculator m_sunCalculator;
    MoonEquatorialCalculator m_moonCalculator;
    PlanetEquatorialCalculator m_planetCalculator;
};

std::unique_ptr<IEphemerisEngine> createEphemerisEngine()
{
    return std::make_unique<SimpleEphemerisEngine>(std::span<const CelestialBody>{});
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngine(const IStarCatalog& catalog)
{
    return createEphemerisEngine(catalog.bodies());
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngine(std::span<const CelestialBody> bodies)
{
    return std::make_unique<SimpleEphemerisEngine>(bodies);
}

}  // namespace skygate::ephemeris
