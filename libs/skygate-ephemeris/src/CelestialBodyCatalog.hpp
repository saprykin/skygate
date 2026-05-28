#pragma once

#include "BaseCelestialBody.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace skygate::ephemeris {

class CelestialBodyCatalog final {
public:
    enum class BodyDomain {
        OwnGalaxy,
        Distant
    };

    struct OrderEntry {
        BodyDomain domain = BodyDomain::OwnGalaxy;
        std::size_t bodyIndex = 0;
    };

    CelestialBodyCatalog() = default;
    CelestialBodyCatalog(const CelestialBodyCatalog& other);
    CelestialBodyCatalog& operator=(const CelestialBodyCatalog& other);
    CelestialBodyCatalog(CelestialBodyCatalog&& other) noexcept;
    CelestialBodyCatalog& operator=(CelestialBodyCatalog&& other) noexcept;
    ~CelestialBodyCatalog();

    explicit CelestialBodyCatalog(std::span<const BaseCelestialBody* const> bodies);
    explicit CelestialBodyCatalog(std::span<const OwnGalaxyCelestialBody> ownGalaxyBodies);
    explicit CelestialBodyCatalog(std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies);
    CelestialBodyCatalog(
        std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies
    );
    CelestialBodyCatalog(
        std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies,
        std::vector<DistantCelestialBody> distantBodies,
        std::vector<OrderEntry> orderedBodyIndexes
    );

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::span<const BaseCelestialBody* const> bodies() const noexcept;
    [[nodiscard]] std::span<const OwnGalaxyCelestialBody> ownGalaxyBodies() const noexcept;
    [[nodiscard]] std::span<const DistantCelestialBody> distantBodies() const noexcept;
    [[nodiscard]] const BaseCelestialBody& bodyAt(std::size_t bodyIndex) const noexcept;

private:
    void rebuildOrderedBodies();
    void rebuildSequentialOrder();

    std::vector<OwnGalaxyCelestialBody> m_ownGalaxyBodies;
    std::vector<DistantCelestialBody> m_distantBodies;
    std::vector<OrderEntry> m_orderedBodyIndexes;
    std::vector<const BaseCelestialBody*> m_orderedBodies;
};

}  // namespace skygate::ephemeris
