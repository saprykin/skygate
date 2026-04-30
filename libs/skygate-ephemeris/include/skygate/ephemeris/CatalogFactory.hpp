#pragma once

#include "skygate/ephemeris/CatalogLoader.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalog(const CatalogSourceRequest& request);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalog(
    CatalogSourceType type,
    std::string_view data = {},
    const HygParseProgressCallback& progressCallback = {},
    const CatalogSelectionOptions& selectionOptions = {}
);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromBodies(std::vector<CelestialBody> bodies);
[[nodiscard]] std::unique_ptr<IStarCatalog> createBundledStarCatalog();
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsv(std::string_view csvData);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsv(
    std::string_view csvData,
    const HygParseProgressCallback& progressCallback
);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsvGzip(std::string_view gzipData);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsvGzip(
    std::string_view gzipData,
    const HygParseProgressCallback& progressCallback
);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromOpenNgcCsv(std::string_view csvData);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromOpenNgcCsv(
    std::string_view csvData,
    const HygParseProgressCallback& progressCallback
);

}  // namespace skygate::ephemeris
