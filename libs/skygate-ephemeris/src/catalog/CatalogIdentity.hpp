#pragma once

#include "skygate/ephemeris/Types.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace skygate::ephemeris::catalog_identity {

[[nodiscard]] bool isReferenceLineStarId(std::string_view id);
[[nodiscard]] bool containsBodyId(
    std::span<const CelestialBody> bodies,
    std::string_view id
);
[[nodiscard]] bool sharesDeepSkyAlias(
    const CelestialBody& lhs,
    const CelestialBody& rhs
);
[[nodiscard]] bool isAnalyticSolarSystemBody(const CelestialBody& body) noexcept;
[[nodiscard]] std::size_t countDeepSkyObjects(std::span<const CelestialBody> bodies);

}  // namespace skygate::ephemeris::catalog_identity
