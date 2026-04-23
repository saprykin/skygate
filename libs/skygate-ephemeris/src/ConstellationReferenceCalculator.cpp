#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] std::string normalizedLookupKey(const std::string_view value)
{
    const auto beginIt = std::find_if_not(
        value.begin(),
        value.end(),
        [](const unsigned char character) {
            return std::isspace(character) != 0;
        }
    );
    const auto endIt = std::find_if_not(
        value.rbegin(),
        value.rend(),
        [](const unsigned char character) {
            return std::isspace(character) != 0;
        }
    ).base();

    if (beginIt >= endIt) {
        return {};
    }

    std::string normalized;
    normalized.reserve(static_cast<std::size_t>(std::distance(beginIt, endIt)));
    std::transform(
        beginIt,
        endIt,
        std::back_inserter(normalized),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return normalized;
}

[[nodiscard]] bool hasFiniteHorizontal(const core::HorizontalCoordinate& horizontal)
{
    return std::isfinite(horizontal.altitudeDeg) && std::isfinite(horizontal.azimuthDeg);
}

}  // namespace

std::optional<core::HorizontalCoordinate> ConstellationReferenceCalculator::labelCenter(
    const SkySnapshot& snapshot,
    const std::span<const ConstellationLabelRef> labelRefs,
    const std::string_view label
)
{
    const std::string normalizedLabel = normalizedLookupKey(label);
    if (normalizedLabel.empty()) {
        return std::nullopt;
    }

    const auto labelRefIt = std::find_if(
        labelRefs.begin(),
        labelRefs.end(),
        [&normalizedLabel](const ConstellationLabelRef& labelRef) {
            return normalizedLookupKey(labelRef.first) == normalizedLabel;
        }
    );
    if (labelRefIt == labelRefs.end()) {
        return std::nullopt;
    }

    std::unordered_map<std::string, core::HorizontalCoordinate> horizontalByBodyId;
    horizontalByBodyId.reserve(snapshot.states.size());
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        horizontalByBodyId.insert({normalizedLookupKey(body.id), state.horizontal});
    }

    core::ProjectionMath::Vec3 sum {0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : labelRefIt->second) {
        const auto horizontalIt = horizontalByBodyId.find(normalizedLookupKey(hipId));
        if (
            horizontalIt == horizontalByBodyId.end()
            || !hasFiniteHorizontal(horizontalIt->second)
        ) {
            continue;
        }

        const auto vector = core::ProjectionMath::horizontalToUnitVector(horizontalIt->second);
        sum[0] += vector[0];
        sum[1] += vector[1];
        sum[2] += vector[2];
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = core::ProjectionMath::normalize(sum);
    if (core::ProjectionMath::length(normalizedVector) <= 0.0) {
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
