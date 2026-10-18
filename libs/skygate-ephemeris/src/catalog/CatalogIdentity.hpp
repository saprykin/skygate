#pragma once

#include "BaseCelestialBody.hpp"

#include <cstddef>
#include <span>

namespace skygate::ephemeris {

class CatalogIdentity final {
public:
    [[nodiscard]] static bool isAnalyticSolarSystemBody(const BaseCelestialBody& body) noexcept;
    [[nodiscard]] static std::size_t countDeepSkyObjects(std::span<const BaseCelestialBody* const> bodies);
};

}  // namespace skygate::ephemeris
