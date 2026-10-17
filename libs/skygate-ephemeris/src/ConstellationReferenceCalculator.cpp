#include "ConstellationReferenceCalculator.hpp"
#include "StringUtilities.hpp"
#include "math/SphericalGeometry.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris {

void ConstellationReferenceCalculator::AnchorCentroidAccumulator::addAnchor(
    const skygate::core::HorizontalCoordinate& horizontal
) noexcept
{
    if (!horizontal.isFinite()) {
        return;
    }

    m_directionSum += skygate::core::SphericalGeometry::horizontalToUnitVector(horizontal);
    ++m_validAnchorCount;
}

std::optional<skygate::core::HorizontalCoordinate>
ConstellationReferenceCalculator::AnchorCentroidAccumulator::centroid() const noexcept
{
    if (m_validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedDirection = m_directionSum.normalized();
    if (!normalizedDirection.has_value()) {
        return std::nullopt;
    }

    return skygate::core::SphericalGeometry::horizontalFromUnitVector(*normalizedDirection);
}

std::optional<skygate::core::HorizontalCoordinate> ConstellationReferenceCalculator::anchorCentroid(
    const EphemerisSnapshot& snapshot,
    const std::span<const ConstellationAnchorGroup> anchorGroups,
    const std::string_view labelName
)
{
    const std::string normalizedLabel = StringUtilities::normalizedLookupKey(labelName);
    if (normalizedLabel.empty()) {
        return std::nullopt;
    }

    const auto anchorGroupIt = std::find_if(
        anchorGroups.begin(), anchorGroups.end(), [&normalizedLabel](const ConstellationAnchorGroup& anchorGroup) {
            return StringUtilities::normalizedLookupKey(anchorGroup.first) == normalizedLabel;
        }
    );
    if (anchorGroupIt == anchorGroups.end()) {
        return std::nullopt;
    }

    std::unordered_map<std::string, skygate::core::HorizontalCoordinate> horizontalByBodyId;
    horizontalByBodyId.reserve(snapshot.states.size());
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        horizontalByBodyId.insert({StringUtilities::normalizedLookupKey(body.id), state.horizontal});
    }

    AnchorCentroidAccumulator accumulator;
    for (const std::string& hipId : anchorGroupIt->second) {
        const auto horizontalIt = horizontalByBodyId.find(StringUtilities::normalizedLookupKey(hipId));
        if (horizontalIt == horizontalByBodyId.end()) {
            continue;
        }

        accumulator.addAnchor(horizontalIt->second);
    }

    return accumulator.centroid();
}

}  // namespace skygate::ephemeris
