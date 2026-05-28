#pragma once

#include "BaseCelestialBody.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class CatalogIdentity final {
public:
    [[nodiscard]] static bool containsBodyId(std::span<const BaseCelestialBody* const> bodies, std::string_view id);
    [[nodiscard]] static bool sharesDeepSkyAlias(const BaseCelestialBody& lhs, const BaseCelestialBody& rhs);
    [[nodiscard]] static bool isAnalyticSolarSystemBody(const BaseCelestialBody& body) noexcept;
    [[nodiscard]] static std::size_t countDeepSkyObjects(std::span<const BaseCelestialBody* const> bodies);
};

}  // namespace skygate::ephemeris
