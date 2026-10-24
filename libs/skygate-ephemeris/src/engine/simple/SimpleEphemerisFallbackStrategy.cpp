#include "SimpleEphemerisFallbackStrategy.hpp"
#include "SimpleEphemerisEngine.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] bool requestsTopocentricState(const EphemerisRequest& request) noexcept
{
    return EphemerisCorrectionFlags::has(
        request.options.correctionFlags(), EphemerisCorrectionFlags::diurnalParallax()
    );
}

}  // namespace

std::optional<CelestialBodyState> SimpleEphemerisFallbackStrategy::computeFallbackState(
    const EphemerisRequest& request, const BaseCelestialBody& body, const std::size_t bodyIndex
) const
{
    CelestialBodyState state = SimpleEphemerisEngine::computeStateForBody(body, bodyIndex, request.context);
    if (state.metadata.status == EphemerisEngineQueryStatus::Type::Unsupported) {
        return std::nullopt;
    }

    // The fallback reports its own metadata: the simple-engine status and
    // warnings do not describe a degraded solar-system fallback.
    state.metadata = EphemerisEngineQueryResult{};
    state.metadata.dataSourceProvenanceKind = EphemerisDataSourceProvenance::Type::SimpleSolarSystemFallback;
    state.metadata.dataSourceProvenance = std::string{
        EphemerisDataSourceProvenance::displayName(EphemerisDataSourceProvenance::Type::SimpleSolarSystemFallback)
    };

    const double notAvailable = std::numeric_limits<double>::quiet_NaN();
    if (!requestsTopocentricState(request)) {
        state.horizontal.altitudeDeg = notAvailable;
        state.horizontal.azimuthDeg = notAvailable;
    }

    EphemerisCorrectionFlags applied = EphemerisCorrectionFlags::geometric();
    if (requestsTopocentricState(request) && request.context.observer.isValid()) {
        applied |= EphemerisCorrectionFlags::diurnalParallax();
    }
    state.metadata.appliedCorrections = applied;

    const EphemerisCorrectionFlags requested = request.options.correctionFlags();
    const EphemerisCorrectionFlags unapplied = EphemerisCorrectionFlags::without(requested, applied);
    if (unapplied != EphemerisCorrectionFlags::noCorrections()) {
        state.metadata.addUnavailableCorrection(unapplied);
    }
    state.metadata.finalizeCorrectionTracking(requested);

    return state;
}

}  // namespace skygate::ephemeris
