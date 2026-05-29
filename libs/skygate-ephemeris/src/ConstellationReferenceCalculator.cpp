#include "ConstellationReferenceCalculator.hpp"
#include "StringUtilities.hpp"
#include "math/AngleMath.hpp"
#include "math/SphericalGeometry.hpp"
#include "math/Vector3d.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris {

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

    skygate::core::Vector3d sum;
    int validAnchorCount = 0;
    for (const std::string& hipId : anchorGroupIt->second) {
        const auto horizontalIt = horizontalByBodyId.find(StringUtilities::normalizedLookupKey(hipId));
        if (horizontalIt == horizontalByBodyId.end() || !horizontalIt->second.isFinite()) {
            continue;
        }

        const auto vector = skygate::core::SphericalGeometry::horizontalToUnitVector(horizontalIt->second);
        sum += vector;
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = sum.normalized();
    if (!normalizedVector.has_value()) {
        return std::nullopt;
    }

    return skygate::core::HorizontalCoordinate{
        .altitudeDeg = skygate::core::AngleMath::toDegrees(std::asin(std::clamp(normalizedVector->z, -1.0, 1.0))),
        .azimuthDeg = skygate::core::AngleMath::normalizeDegrees(
            skygate::core::AngleMath::toDegrees(std::atan2(normalizedVector->x, normalizedVector->y))
        )
    };
}

}  // namespace skygate::ephemeris
