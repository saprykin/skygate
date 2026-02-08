#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstdint>
#include <functional>
#include <cstddef>
#include <memory>
#include <string_view>

namespace skygate::ephemeris {

using HygParseProgressCallback = std::function<void(std::size_t parsedObjectCount)>;

enum class CatalogSourceType : std::uint8_t {
    Bundled,
    Rows,
    HygCsv,
    HygCsvGzip
};

struct CatalogSourceRequest {
    CatalogSourceType type = CatalogSourceType::Bundled;
    std::string_view data;
    HygParseProgressCallback progressCallback;
};

[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalog(const CatalogSourceRequest& request);
[[nodiscard]] std::unique_ptr<IStarCatalog> createStarCatalog(
    CatalogSourceType type,
    std::string_view data = {},
    const HygParseProgressCallback& progressCallback = {}
);
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
