#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "time/TimeScale.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace skygate::ephemeris {

class DistantCelestialBody;
class OwnGalaxyCelestialBody;

// Model-owned normalization/validation boundary for immutable catalog
// snapshots.
//
// Parsed results, vector factories, existing-catalog factories, composition,
// and cache deserialization all funnel through this class so that enum
// values, order indexes, identity consistency, numeric domains, and
// capability metadata are validated consistently.
//
// Normalization applied to all inputs:
//
//   - canonical sun/moon IDs are reclassified to their analytic kinds,
//   - external identifiers are re-normalized per namespace and incomplete
//     entries are dropped,
//   - empty aliases are dropped and remaining aliases are whitespace-trimmed.
//
// This class validates per-body source validity only. Raw duplicate source
// rows remain legal here; deduplication to the unique active identity is the
// responsibility of the identity index and composition merger (CAT-12).
class CatalogSnapshotValidator final {
public:
    struct Report {
        bool ok = true;
        std::string errorDetail;
    };

    // Enum and order-validity predicates shared with wire-format decoding so
    // the binary codec does not keep a second copy of the same policy.
    [[nodiscard]] static bool isKnownBodyKind(BaseCelestialBody::Kind kind) noexcept;
    [[nodiscard]] static bool isKnownDeepSkyObjectKind(DeepSkyObjectInfo::Kind kind) noexcept;
    [[nodiscard]] static bool isKnownTimeScale(skygate::core::TimeScale timeScale) noexcept;
    [[nodiscard]] static bool isOrderEntryValid(
        CelestialBodyCatalog::BodyDomain domain,
        std::size_t bodyIndex,
        std::size_t ownGalaxyBodyCount,
        std::size_t distantBodyCount
    ) noexcept;

    // Normalizes bodies in place and validates the single-domain snapshot
    // inputs used by the vector factory.
    static Report validate(std::vector<OwnGalaxyCelestialBody>& bodies);

    // Normalizes bodies in place and validates storage-domain vectors plus
    // explicit order entries before a snapshot is materialized.
    static Report validate(
        std::vector<OwnGalaxyCelestialBody>& ownGalaxyBodies,
        std::vector<DistantCelestialBody>& distantBodies,
        const std::vector<CelestialBodyCatalog::OrderEntry>& orderedBodyIndexes
    );
};

}  // namespace skygate::ephemeris
