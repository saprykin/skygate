#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

namespace skygate::ephemeris {

class EphemerisEngineStub final : public IEphemerisEngine {
public:
    explicit EphemerisEngineStub(const IStarCatalog* catalog)
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

        for (const auto& body : bodies) {
            CelestialBodyState state;
            state.body = body;
            state.equatorial = {};
            state.horizontal = {};
            snapshot.states.push_back(state);
        }

        return snapshot;
    }

private:
    const IStarCatalog* m_catalog = nullptr;
};

std::unique_ptr<IEphemerisEngine> createEphemerisEngineStub(const IStarCatalog* catalog)
{
    return std::make_unique<EphemerisEngineStub>(catalog);
}

}  // namespace skygate::ephemeris
