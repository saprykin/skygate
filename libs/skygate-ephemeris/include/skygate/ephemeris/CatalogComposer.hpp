#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace skygate::ephemeris {

enum class CatalogCompositionSource : std::uint8_t {
    Primary,
    DeepSky,
    BuiltInEphemeris
};

struct ActiveCatalogCompositionRequest final {
    const IStarCatalog& sourceCatalog;
    const IStarCatalog* deepSkyCatalog = nullptr;
    bool useBundledDeepSkyCatalog = false;
    std::size_t currentConstellationCount = 0;
    std::size_t knownDeepSkyObjectCount = 0;
};

struct ActiveCatalogCompositionResult final {
    std::unique_ptr<IStarCatalog> catalog;
    std::vector<CatalogCompositionSource> sourceKinds;
    std::size_t bodyCount = 0;
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;

    [[nodiscard]] bool isSuccess() const noexcept;
};

[[nodiscard]] ActiveCatalogCompositionResult composeActiveCatalog(
    const ActiveCatalogCompositionRequest& request
);

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
