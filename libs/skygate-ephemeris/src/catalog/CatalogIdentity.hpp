#pragma once

#include "Types.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class CatalogIdentity final {
public:
    [[nodiscard]] static bool containsBodyId(std::span<const CelestialBody> bodies, std::string_view id);
    [[nodiscard]] static bool sharesDeepSkyAlias(const CelestialBody& lhs, const CelestialBody& rhs);
    [[nodiscard]] static bool isAnalyticSolarSystemBody(const CelestialBody& body) noexcept;
    [[nodiscard]] static std::size_t countDeepSkyObjects(std::span<const CelestialBody> bodies);
};

}  // namespace skygate::ephemeris
