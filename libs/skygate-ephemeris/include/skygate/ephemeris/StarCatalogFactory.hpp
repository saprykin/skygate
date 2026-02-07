#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <functional>
#include <memory>
#include <cstddef>
#include <string_view>

namespace skygate::ephemeris {

using HygParseProgressCallback = std::function<void(std::size_t parsedObjectCount)>;

[[nodiscard]] std::unique_ptr<IStarCatalog> createBundledStarCatalog();
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromRows(std::string_view catalogRows);
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

}  // namespace skygate::ephemeris
