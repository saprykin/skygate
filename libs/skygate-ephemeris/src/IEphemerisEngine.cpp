#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include "common/StringUtilities.hpp"

namespace skygate::ephemeris {

std::optional<CelestialBodyState> IEphemerisEngine::computeBodyState(
    const core::SkyContext& context,
    const std::string_view bodyId
) const
{
    const SkySnapshot snapshot = compute(context);
    for (const CelestialBodyState& state : snapshot.states) {
        const CelestialBody& body = snapshot.bodyAt(state.bodyIndex);
        if (strings::equalsIgnoreAsciiCase(body.id, bodyId)) {
            return state;
        }
    }

    return std::nullopt;
}

std::optional<CelestialBodyState> IEphemerisEngine::computeBodyState(
    const core::SkyContext& context,
    const std::uint32_t bodyIndex
) const
{
    const SkySnapshot snapshot = compute(context);
    for (const CelestialBodyState& state : snapshot.states) {
        if (state.bodyIndex == bodyIndex) {
            return state;
        }
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
