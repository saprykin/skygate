#pragma once

#include "skygate/ephemeris/CatalogLoadResult.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

using HygParseProgressCallback = std::function<void(std::size_t parsedObjectCount)>;

enum class CatalogSelectionMode : std::uint8_t {
    None,
    BrightestByVisualMagnitude
};

struct CatalogSelectionOptions {
    CatalogSelectionMode mode = CatalogSelectionMode::None;
    std::size_t maxBodyCount = 0;

    [[nodiscard]] bool isEnabled() const noexcept
    {
        return mode != CatalogSelectionMode::None && maxBodyCount > 0;
    }
};

enum class CatalogSourceType : std::uint8_t {
    Bundled,
    HygCsv,
    HygCsvGzip,
    OpenNgcCsv
};

struct CatalogSourceRequest {
    CatalogSourceType type = CatalogSourceType::Bundled;
    std::string_view data;
    HygParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
};

[[nodiscard]] CatalogLoadResult loadStarCatalog(const CatalogSourceRequest& request);
[[nodiscard]] CatalogLoadResult loadStarCatalog(
    CatalogSourceType type,
    std::string_view data = {},
    const HygParseProgressCallback& progressCallback = {},
    const CatalogSelectionOptions& selectionOptions = {}
);
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
