#pragma once

#include "CatalogLoadResult.hpp"
#include "CatalogSourceRequest.hpp"

#include <string_view>

namespace skygate::ephemeris {

// Front door for loading an already-decoded payload of a known schema.
//
// The registered schema parser (CatalogSchemaRegistry) produces catalog
// bodies; the loader finalizes them into an eager IStarCatalog snapshot,
// validates the snapshot, applies optional brightest-N selection, and reports
// diagnostics. An unknown schema fails with
// CatalogLoadResult::ErrorCode::UnsupportedFormat. Container decoding,
// detection, and parse options belong to CatalogPayloadParser.
class CatalogLoader final {
public:
    [[nodiscard]] static CatalogLoadResult load(const CatalogSourceRequest& request);
    [[nodiscard]] static CatalogLoadResult load(
        CatalogSourceType type,
        std::string_view data = {},
        const CatalogParseProgressCallback& progressCallback = {},
        const CatalogSelectionOptions& selectionOptions = {}
    );
};

}  // namespace skygate::ephemeris
