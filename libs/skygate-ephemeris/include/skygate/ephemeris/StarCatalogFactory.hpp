#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <string_view>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IStarCatalog> createBundledStarCatalog();
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromRows(std::string_view catalogRows);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsv(std::string_view csvData);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsvGzip(std::string_view gzipData);

}  // namespace skygate::ephemeris
