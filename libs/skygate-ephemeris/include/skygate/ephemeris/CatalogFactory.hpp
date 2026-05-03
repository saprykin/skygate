#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <vector>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromBodies(std::vector<CelestialBody> bodies);
[[nodiscard]] std::unique_ptr<IStarCatalog> createBundledStarCatalog();

}  // namespace skygate::ephemeris
