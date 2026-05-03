#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
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

}  // namespace skygate::ephemeris
