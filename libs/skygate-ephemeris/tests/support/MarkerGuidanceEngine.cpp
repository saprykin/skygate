#include "MarkerGuidanceEngine.hpp"

#include "UtcTimeCodec.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace skygate::ephemeris::tests {

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

MarkerGuidanceEngine::MarkerGuidanceEngine(std::shared_ptr<int> sampleCount) : m_sampleCount(std::move(sampleCount)) {}

EphemerisEngineKind::Type MarkerGuidanceEngine::kind() const noexcept
{
    return EphemerisEngineKind::Type::Simple;
}

std::string_view MarkerGuidanceEngine::name() const noexcept
{
    return "Marker guidance test engine";
}

EphemerisSnapshot MarkerGuidanceEngine::compute(const EphemerisRequest& request) const
{
    return compute(request.context);
}

std::optional<CelestialBodyState>
MarkerGuidanceEngine::computeBodyState(const EphemerisRequest& request, const std::string_view) const
{
    return computeBodyState(request, std::size_t{0U});
}

std::optional<CelestialBodyState>
MarkerGuidanceEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    ++*m_sampleCount;
    const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(request.context.utcTime);
    const double phase = std::fmod(seconds, 86400.0) / 86400.0;
    return CelestialBodyState{
        .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
        .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
        .horizontal = {.altitudeDeg = 35.0 * std::sin(2.0 * kPi * (phase - 0.25)), .azimuthDeg = 180.0}
    };
}

EphemerisSnapshot MarkerGuidanceEngine::compute(const skygate::core::ObservationContext&) const
{
    return {};
}

std::optional<CelestialBodyState>
MarkerGuidanceEngine::computeBodyState(const skygate::core::ObservationContext&, const std::string_view) const
{
    return std::nullopt;
}

std::optional<CelestialBodyState>
MarkerGuidanceEngine::computeBodyState(const skygate::core::ObservationContext&, std::size_t) const
{
    return std::nullopt;
}

}  // namespace skygate::ephemeris::tests
