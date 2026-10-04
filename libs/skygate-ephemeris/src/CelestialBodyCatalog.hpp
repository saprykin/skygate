#pragma once

#include "BaseCelestialBody.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace skygate::ephemeris {

// Eager, immutable snapshot of catalog bodies with two storage domains.
//
// The catalog owns its bodies by value. bodies() is the common body view for
// consumers: a span of BaseCelestialBody pointers in the append/order
// sequence. ownGalaxyBodies(), distantBodies(), and orderedBodyIndexes() are
// representation-specific accessors used by model construction and
// serialization; ordinary consumers should read bodies().
//
// The two storage domains are operational, not a statement of physical
// membership:
//
//   - OwnGalaxy holds non-deep-sky bodies (stars, planets, moons, the Sun,
//     and constellations), whose optional metadata is a fixed equatorial
//     coordinate and/or star astrometry.
//   - Distant holds deep-sky objects, whose optional metadata is a
//     DeepSkyObjectInfo record. Galactic nebulae and clusters are deep-sky
//     objects and therefore live in Distant, even though they are members of
//     our galaxy.
//
// Lifecycle: bodies(), bodyAt(), and the domain spans point into the catalog's
// internal vectors and remain valid until the catalog is destroyed, moved
// from, or mutated by appendBody(). Copying or moving the catalog transfers
// that storage, so the new owner's spans refer to the transferred vectors.
// Constructing a catalog from a body span deep-copies each body, so a
// materialized snapshot stays valid independently of the source owner.
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
    [[nodiscard]] std::span<const OrderEntry> orderedBodyIndexes() const noexcept;
    [[nodiscard]] const BaseCelestialBody& bodyAt(std::size_t bodyIndex) const noexcept;

    // Copies `body` into the storage domain matching its kind and appends an
    // order entry, preserving the append sequence as the observable body
    // order. This is the authoritative body copy/append operation used by
    // snapshot construction, selection, augmentation, and merging.
    void appendBody(const BaseCelestialBody& body);

    [[nodiscard]] static OwnGalaxyCelestialBody copyOwnGalaxyBody(const BaseCelestialBody& body);
    [[nodiscard]] static DistantCelestialBody copyDistantBody(const BaseCelestialBody& body);

private:
    void appendBodyEntry(const BaseCelestialBody& body);
    void rebuildOrderedBodies();
    void rebuildSequentialOrder();

    std::vector<OwnGalaxyCelestialBody> m_ownGalaxyBodies;
    std::vector<DistantCelestialBody> m_distantBodies;
    std::vector<OrderEntry> m_orderedBodyIndexes;
    std::vector<const BaseCelestialBody*> m_orderedBodies;
};

}  // namespace skygate::ephemeris
