#pragma once

#include "EphemerisSnapshot.hpp"
#include "HorizontalCoordinate.hpp"
#include "catalog/constellation/ConstellationData.hpp"
#include "math/Vector3d.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class ConstellationReferenceCalculator final {
public:
    // Collects the finite anchor directions of a constellation and resolves
    // their normalized centroid direction. Callers keep their own body lookup.
    class AnchorCentroidAccumulator final {
    public:
        void addAnchor(const skygate::core::HorizontalCoordinate& horizontal) noexcept;
        [[nodiscard]] std::optional<skygate::core::HorizontalCoordinate> centroid() const noexcept;

    private:
        skygate::core::Vector3d m_directionSum;
        int m_validAnchorCount = 0;
    };

    [[nodiscard]] static std::optional<skygate::core::HorizontalCoordinate> anchorCentroid(
        const EphemerisSnapshot& snapshot,
        std::span<const ConstellationAnchorGroup> anchorGroups,
        std::string_view labelName
    );
};

}  // namespace skygate::ephemeris
