#include "TrailBuilderTestEngine.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace skygate::ui::tests {

TrailBuilderTestEngine::TrailBuilderTestEngine(const std::uint32_t expectedBodyIndex)
    : m_expectedBodyIndex(expectedBodyIndex)
{
}

skygate::ephemeris::EphemerisEngineKind::Type TrailBuilderTestEngine::kind() const noexcept
{
    return m_nonEnumeratedKind ? static_cast<skygate::ephemeris::EphemerisEngineKind::Type>(42)
                               : skygate::ephemeris::EphemerisEngineKind::Type::Simple;
}

std::string_view TrailBuilderTestEngine::name() const noexcept
{
    return "Trail test engine";
}

skygate::ephemeris::EphemerisEngineTraits TrailBuilderTestEngine::traits() const noexcept
{
    return m_traits;
}

void TrailBuilderTestEngine::setHighPrecisionTraits(const bool highPrecisionTraits) noexcept
{
    m_traits = highPrecisionTraits ? skygate::ephemeris::EphemerisEngineTraits::highPrecisionEngine()
                                   : skygate::ephemeris::EphemerisEngineTraits::noTraits();
}

void TrailBuilderTestEngine::setTraits(const skygate::ephemeris::EphemerisEngineTraits traits) noexcept
{
    m_traits = traits;
}

void TrailBuilderTestEngine::setNonEnumeratedKind(const bool nonEnumeratedKind) noexcept
{
    m_nonEnumeratedKind = nonEnumeratedKind;
}

void TrailBuilderTestEngine::setGapAtPresent(const bool gapAtPresent) noexcept
{
    m_gapAtPresent = gapAtPresent;
}

skygate::ephemeris::EphemerisSnapshot
TrailBuilderTestEngine::compute(const skygate::ephemeris::EphemerisRequest& request) const
{
    return compute(request.context);
}

std::optional<skygate::ephemeris::CelestialBodyState> TrailBuilderTestEngine::computeBodyState(
    const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId
) const
{
    (void)bodyId;
    return computeBodyState(request, std::size_t{m_expectedBodyIndex});
}

std::optional<skygate::ephemeris::CelestialBodyState> TrailBuilderTestEngine::computeBodyState(
    const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex
) const
{
    ++m_requestBodyStateCalls;
    m_lastRequest = request;
    m_requestOffsetMinutes.push_back(
        static_cast<int>(
            std::chrono::duration_cast<std::chrono::minutes>(request.context.utcTime.time_since_epoch()).count()
        )
    );
    return stateForContext(request.context, static_cast<std::uint32_t>(bodyIndex));
}

skygate::ephemeris::EphemerisSnapshot
TrailBuilderTestEngine::compute(const skygate::core::ObservationContext& context) const
{
    (void)context;
    return {};
}

std::optional<skygate::ephemeris::CelestialBodyState>
TrailBuilderTestEngine::computeBodyState(const skygate::core::ObservationContext&, std::string_view) const
{
    return std::nullopt;
}

std::optional<skygate::ephemeris::CelestialBodyState> TrailBuilderTestEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::size_t bodyIndex
) const
{
    ++m_contextBodyStateCalls;
    return stateForContext(context, static_cast<std::uint32_t>(bodyIndex));
}

std::optional<skygate::ephemeris::CelestialBodyState> TrailBuilderTestEngine::stateForContext(
    const skygate::core::ObservationContext& context, const std::uint32_t bodyIndex
) const
{
    m_sawExpectedBodyIndex = m_sawExpectedBodyIndex || bodyIndex == m_expectedBodyIndex;

    const auto offsetMinutes =
        static_cast<int>(std::chrono::duration_cast<std::chrono::minutes>(context.utcTime.time_since_epoch()).count());
    if (m_gapAtPresent && offsetMinutes == 0) {
        return std::nullopt;
    }

    return skygate::ephemeris::CelestialBodyState{
        .bodyIndex = bodyIndex,
        .horizontal = {
            .altitudeDeg = 45.0 + (static_cast<double>(offsetMinutes) / 6000.0),
            .azimuthDeg = 180.0 + (static_cast<double>(offsetMinutes) / 6000.0)
        }
    };
}

bool TrailBuilderTestEngine::sawExpectedBodyIndex() const noexcept
{
    return m_sawExpectedBodyIndex;
}

int TrailBuilderTestEngine::requestBodyStateCalls() const noexcept
{
    return m_requestBodyStateCalls;
}

int TrailBuilderTestEngine::contextBodyStateCalls() const noexcept
{
    return m_contextBodyStateCalls;
}

const std::optional<skygate::ephemeris::EphemerisRequest>& TrailBuilderTestEngine::lastRequest() const noexcept
{
    return m_lastRequest;
}

const std::vector<int>& TrailBuilderTestEngine::requestOffsetMinutes() const noexcept
{
    return m_requestOffsetMinutes;
}

}  // namespace skygate::ui::tests
