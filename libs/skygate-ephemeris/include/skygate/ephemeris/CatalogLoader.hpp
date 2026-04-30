#pragma once

#include "skygate/ephemeris/CatalogLoadResult.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

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

}  // namespace skygate::ephemeris
