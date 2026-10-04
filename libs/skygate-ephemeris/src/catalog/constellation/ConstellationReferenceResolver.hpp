#pragma once

#include "catalog/CatalogIdentityIndex.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

// Resolves constellation reference strings emitted by the Stellarium adapters
// (the canonical "hip_<n>" spelling) to the canonical body IDs of the active
// catalog through the shared identity index.
//
// Rendering, centroid calculation, search, and selection consume the resolved
// references instead of interpreting or normalizing the HIP spelling
// themselves. Build one resolver per active catalog revision and cache its
// output; do not rebuild the identity index per frame.
class ConstellationReferenceResolver final {
public:
    enum class Status {
        Resolved,
        Unresolved,
        Ambiguous
    };

    struct Resolution final {
        Status status = Status::Unresolved;
        // Canonical body id of the matched body when status is Resolved.
        std::string bodyId;
        // The original reference spelling, preserved for diagnostics.
        std::string reference;
    };

    explicit ConstellationReferenceResolver(std::span<const BaseCelestialBody* const> bodies);

    [[nodiscard]] Resolution resolve(std::string_view reference) const;

    // Resolves every endpoint of every line segment. Segments with an
    // unresolved or ambiguous endpoint are dropped and reported once.
    [[nodiscard]] std::vector<ConstellationLineRef> resolveLines(std::span<const ConstellationLineRef> lineRefs) const;

    // Resolves every anchor of every group. Unresolved or ambiguous anchors
    // are dropped; groups left without any resolved anchor are dropped. The
    // surviving anchor lists contain canonical body IDs.
    [[nodiscard]] std::vector<ConstellationAnchorGroup>
    resolveAnchors(std::span<const ConstellationAnchorGroup> anchorGroups) const;

private:
    std::span<const BaseCelestialBody* const> m_bodies;
    CatalogIdentityIndex m_index;
};

}  // namespace skygate::ephemeris
