#include "SimpleEphemerisFallbackStrategy.hpp"
#include "EquatorialToHorizontalCalculator.hpp"
#include "MoonEquatorialCalculator.hpp"
#include "PlanetEquatorialCalculator.hpp"
#include "SunEquatorialCalculator.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
computeEquatorial(const BaseCelestialBody& body, const skygate::core::UtcTimePoint& utcTime) noexcept
{
    if (body.fixedEquatorialValue().has_value()) {
        return body.fixedEquatorialValue();
    }

    switch (body.kind) {
    case BaseCelestialBody::Kind::Sun:
        return SunEquatorialCalculator{}.compute(utcTime);
    case BaseCelestialBody::Kind::Moon:
        return MoonEquatorialCalculator{}.compute(utcTime);
    case BaseCelestialBody::Kind::Planet:
        return PlanetEquatorialCalculator{}.compute(body.id, utcTime);
    case BaseCelestialBody::Kind::Star:
    case BaseCelestialBody::Kind::Constellation:
    case BaseCelestialBody::Kind::DeepSkyObject:
        break;
    }

    return std::nullopt;
}

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
    const std::optional<skygate::core::EquatorialCoordinate> equatorial =
        computeEquatorial(body, request.context.utcTime);
    if (!equatorial.has_value()) {
        return std::nullopt;
    }

    CelestialBodyState state;
    state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
    state.equatorial = *equatorial;
    state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
    state.metadata.dataSourceProvenanceKind = EphemerisDataSourceProvenance::Type::SimpleSolarSystemFallback;
    state.metadata.dataSourceProvenance = std::string{
        EphemerisDataSourceProvenance::displayName(EphemerisDataSourceProvenance::Type::SimpleSolarSystemFallback)
    };

    const EphemerisCorrectionFlags requested = request.options.correctionFlags();
    EphemerisCorrectionFlags applied = EphemerisCorrectionFlags::geometric();
    if (requestsTopocentricState(request) && request.context.observer.isValid()) {
        state.horizontal =
            EquatorialToHorizontalCalculator::compute(*equatorial, request.context.observer, request.context.utcTime);
        applied |= EphemerisCorrectionFlags::diurnalParallax();
    }

    state.metadata.appliedCorrections = applied;
    const EphemerisCorrectionFlags unapplied = EphemerisCorrectionFlags::without(requested, applied);
    if (unapplied != EphemerisCorrectionFlags::noCorrections()) {
        state.metadata.addUnavailableCorrection(unapplied);
    }
    state.metadata.finalizeCorrectionTracking(requested);

    return state;
}

}  // namespace skygate::ephemeris
