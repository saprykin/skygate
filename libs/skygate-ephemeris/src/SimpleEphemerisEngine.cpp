#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include "common/StringUtilities.hpp"
#include "engine/EquatorialToHorizontalCalculator.hpp"
#include "engine/MoonEquatorialCalculator.hpp"
#include "engine/PlanetEquatorialCalculator.hpp"
#include "engine/SunEquatorialCalculator.hpp"

#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

class SimpleEphemerisEngine final : public IEphemerisEngine {
public:
    explicit SimpleEphemerisEngine(std::span<const CelestialBody> bodies)
        : m_bodies(std::make_shared<const std::vector<CelestialBody>>(bodies.begin(), bodies.end()))
    {
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

    [[nodiscard]] std::optional<CelestialBodyState> computeBodyState(
        const core::SkyContext& context,
        const std::string_view bodyId
    ) const override
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

    [[nodiscard]] std::optional<CelestialBodyState> computeBodyState(
        const core::SkyContext& context,
        const std::uint32_t bodyIndex
    ) const override
    {
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }

        return computeStateForBody((*m_bodies)[bodyIndex], bodyIndex, context);
    }

private:
    [[nodiscard]] CelestialBodyState computeStateForBody(
        const CelestialBody& body,
        const std::size_t bodyIndex,
        const core::SkyContext& context
    ) const
    {
        CelestialBodyState state;
        state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
        state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
        state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();

        if (const auto equatorial = computeEquatorial(body, context.utcTime); equatorial.has_value()) {
            state.equatorial = *equatorial;
            if (context.observer.isValid()) {
                state.horizontal = EquatorialToHorizontalCalculator::compute(
                    *equatorial,
                    context.observer,
                    context.utcTime
                );
            }
        }

        return state;
    }

    [[nodiscard]] std::optional<core::EquatorialCoordinate> computeEquatorial(
        const CelestialBody& body,
        const core::UtcTimePoint& utcTime
    ) const
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
    return std::make_unique<SimpleEphemerisEngine>(std::span<const CelestialBody> {});
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
