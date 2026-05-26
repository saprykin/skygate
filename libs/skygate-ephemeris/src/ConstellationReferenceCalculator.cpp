#include "ConstellationReferenceCalculator.hpp"
#include "StringUtilities.hpp"
#include "math/AngleMath.hpp"
#include "math/SphericalGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris {

std::optional<core::HorizontalCoordinate> ConstellationReferenceCalculator::anchorCentroid(
    const SkySnapshot& snapshot,
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

    std::unordered_map<std::string, core::HorizontalCoordinate> horizontalByBodyId;
    horizontalByBodyId.reserve(snapshot.states.size());
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        horizontalByBodyId.insert({StringUtilities::normalizedLookupKey(body.id), state.horizontal});
    }

    core::SphericalGeometry::Vector3d sum{0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : anchorGroupIt->second) {
        const auto horizontalIt = horizontalByBodyId.find(StringUtilities::normalizedLookupKey(hipId));
        if (horizontalIt == horizontalByBodyId.end() || !horizontalIt->second.isFinite()) {
            continue;
        }

        const auto vector = core::SphericalGeometry::horizontalToUnitVector(horizontalIt->second);
        sum[0] += vector[0];
        sum[1] += vector[1];
        sum[2] += vector[2];
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = core::SphericalGeometry::normalize(sum);
    if (core::SphericalGeometry::length(normalizedVector) <= 0.0) {
        return std::nullopt;
    }

    return core::HorizontalCoordinate{
        .altitudeDeg = core::AngleMath::toDegrees(std::asin(std::clamp(normalizedVector[2], -1.0, 1.0))),
        .azimuthDeg = core::AngleMath::normalizeDegrees(
            core::AngleMath::toDegrees(std::atan2(normalizedVector[0], normalizedVector[1]))
        )
    };
}

}  // namespace skygate::ephemeris
