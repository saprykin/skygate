#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <span>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IStarCatalog> createCatalogByMergingDeepSkyObjects(
    const IStarCatalog& baseCatalog,
    const IStarCatalog& deepSkyCatalog
);
[[nodiscard]] std::unique_ptr<IStarCatalog> createCatalogByMergingDeepSkyObjects(
    std::span<const CelestialBody> baseBodies,
    std::span<const CelestialBody> deepSkyBodies
);
[[nodiscard]] std::unique_ptr<IStarCatalog> createCatalogWithCoreSolarSystemBodies(
    const IStarCatalog& catalog
);
[[nodiscard]] std::unique_ptr<IStarCatalog> createCatalogWithCoreSolarSystemBodies(
    std::span<const CelestialBody> bodies
);

}  // namespace skygate::ephemeris
