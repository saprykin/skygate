#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"

#include "common/StringUtilities.hpp"
#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/SphericalGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris {

std::optional<core::HorizontalCoordinate> ConstellationReferenceCalculator::labelCenter(
    const SkySnapshot& snapshot,
    const std::span<const ConstellationLabelRef> labelRefs,
    const std::string_view label
)
{
    const std::string normalizedLabel = strings::normalizedLookupKey(label);
    if (normalizedLabel.empty()) {
        return std::nullopt;
    }

    const auto labelRefIt = std::find_if(
        labelRefs.begin(),
        labelRefs.end(),
        [&normalizedLabel](const ConstellationLabelRef& labelRef) {
            return strings::normalizedLookupKey(labelRef.first) == normalizedLabel;
        }
    );
    if (labelRefIt == labelRefs.end()) {
        return std::nullopt;
    }

    std::unordered_map<std::string, core::HorizontalCoordinate> horizontalByBodyId;
    horizontalByBodyId.reserve(snapshot.states.size());
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        horizontalByBodyId.insert({strings::normalizedLookupKey(body.id), state.horizontal});
    }

    core::SphericalGeometry::Vector3d sum {0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : labelRefIt->second) {
        const auto horizontalIt = horizontalByBodyId.find(strings::normalizedLookupKey(hipId));
        if (
            horizontalIt == horizontalByBodyId.end()
            || !horizontalIt->second.isFinite()
        ) {
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

    return core::HorizontalCoordinate {
        .altitudeDeg = core::AngleMath::toDegrees(std::asin(std::clamp(
            normalizedVector[2],
            -1.0,
            1.0
        ))),
        .azimuthDeg = core::AngleMath::normalizeDegrees(
            core::AngleMath::toDegrees(std::atan2(normalizedVector[0], normalizedVector[1]))
        )
    };
}

}  // namespace skygate::ephemeris
