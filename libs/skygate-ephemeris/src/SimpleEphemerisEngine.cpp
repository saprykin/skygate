#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include "engine/CoordinateTransform.hpp"
#include "engine/KnownConstellationLookup.hpp"
#include "engine/MoonEquatorialCalculator.hpp"
#include "engine/PlanetEquatorialCalculator.hpp"
#include "engine/StarEquatorialCalculator.hpp"
#include "engine/SunEquatorialCalculator.hpp"

#include <limits>
#include <memory>
#include <optional>

namespace skygate::ephemeris {

class SimpleEphemerisEngine final : public IEphemerisEngine {
public:
    explicit SimpleEphemerisEngine(const IStarCatalog* catalog)
        : m_catalog(catalog)
    {
    }

    [[nodiscard]] SkySnapshot compute(const core::SkyContext& context) const override
    {
        SkySnapshot snapshot;
        snapshot.context = context;

        if (m_catalog == nullptr) {
            return snapshot;
        }

        const auto bodies = m_catalog->bodies();
        snapshot.states.reserve(bodies.size());
        for (const CelestialBody& body : bodies) {
            CelestialBodyState state;
            state.body = body;
            state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
            state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
            state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
            state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();

            if (const auto equatorial = computeEquatorial(body, context.utcTime); equatorial.has_value()) {
                state.equatorial = *equatorial;
                if (context.observer.isValid()) {
                    state.horizontal = CoordinateTransform::equatorialToHorizontal(
                        *equatorial,
                        context.observer,
                        context.utcTime
                    );
                }
            }

            snapshot.states.push_back(state);
        }

        return snapshot;
    }

private:
    [[nodiscard]] std::optional<core::EquatorialCoordinate> computeEquatorial(
        const CelestialBody& body,
        const core::UtcTimePoint& utcTime
    ) const
    {
        if (body.fixedEquatorial.has_value()) {
            return body.fixedEquatorial;
        }

        if (body.type == CelestialBodyType::Sun || body.id == "sun") {
            return m_sunCalculator.compute(utcTime);
        }
        if (body.type == CelestialBodyType::Moon || body.id == "moon") {
            return m_moonCalculator.compute(utcTime);
        }
        if (body.type == CelestialBodyType::Planet) {
            return m_planetCalculator.compute(body.id, utcTime);
        }
        if (body.type == CelestialBodyType::Star) {
            return m_starCalculator.compute(body.id);
        }
        if (body.type == CelestialBodyType::Constellation) {
            return m_knownConstellationLookup.equatorialById(body.id);
        }

        return std::nullopt;
    }

    const IStarCatalog* m_catalog = nullptr;
    SunEquatorialCalculator m_sunCalculator;
    MoonEquatorialCalculator m_moonCalculator;
    PlanetEquatorialCalculator m_planetCalculator;
    StarEquatorialCalculator m_starCalculator;
    KnownConstellationLookup m_knownConstellationLookup;
};

std::unique_ptr<IEphemerisEngine> createEphemerisEngine(const IStarCatalog* catalog)
{
    return std::make_unique<SimpleEphemerisEngine>(catalog);
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngineStub(const IStarCatalog* catalog)
{
    return createEphemerisEngine(catalog);
}

}  // namespace skygate::ephemeris
