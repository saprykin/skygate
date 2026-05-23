#pragma once

#include "skygate/ephemeris/CatalogCompositionSource.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace skygate::ephemeris {

struct ActiveCatalogCompositionResult final {
    std::unique_ptr<IStarCatalog> catalog;
    std::vector<CatalogCompositionSource> sourceKinds;
    std::size_t bodyCount = 0;
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
