#pragma once

#include "IStarCatalog.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace skygate::ephemeris {

// Result of composing an ordered source collection.
//
// sourceIds is parallel to catalog->bodies(): each entry is the stable source
// instance ID of the source that contributed the surviving body. When a body
// is merged from several sources, the winning source's ID is recorded.
struct CatalogCompositionResult final {
    std::unique_ptr<IStarCatalog> catalog;
    std::vector<std::string> sourceIds;
    std::size_t bodyCount = 0;
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
