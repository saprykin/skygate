#include "skygate/ephemeris/EphemerisEngineQueries.hpp"

#include "common/StringUtilities.hpp"

namespace skygate::ephemeris {

std::optional<CelestialBodyState> EphemerisEngineQueries::findBodyStateById(
    const SkySnapshot& snapshot,
    const std::string_view bodyId
)
{
    for (const CelestialBodyState& state : snapshot.states) {
        const CelestialBody& body = snapshot.bodyAt(state.bodyIndex);
        if (strings::equalsIgnoreAsciiCase(body.id, bodyId)) {
            return state;
        }
    }

    return std::nullopt;
}

std::optional<CelestialBodyState> EphemerisEngineQueries::findBodyStateByIndex(
    const SkySnapshot& snapshot,
    const std::uint32_t bodyIndex
)
{
    for (const CelestialBodyState& state : snapshot.states) {
        if (state.bodyIndex == bodyIndex) {
            return state;
        }
    }

    return std::nullopt;
}

std::optional<CelestialBodyState> EphemerisEngineQueries::computeBodyStateById(
    const IEphemerisEngine& engine,
    const core::SkyContext& context,
    const std::string_view bodyId
)
{
    return findBodyStateById(engine.compute(context), bodyId);
}

std::optional<CelestialBodyState> EphemerisEngineQueries::computeBodyStateByIndex(
    const IEphemerisEngine& engine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex
)
{
    return findBodyStateByIndex(engine.compute(context), bodyIndex);
}

}  // namespace skygate::ephemeris
