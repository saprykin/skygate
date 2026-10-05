#pragma once

#include "CatalogSelectionOptions.hpp"
#include "CatalogSourceRequest.hpp"
#include "CatalogSourceType.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

struct CatalogParseRequest {
    std::string_view payload;
    CatalogParseProgressCallback progressCallback;
    CatalogSelectionOptions selectionOptions;
    // Optional archive member name. When present, the named ZIP member is
    // selected exactly and must exist; otherwise the unique supported member is
    // chosen deterministically and ambiguity is reported.
    std::optional<std::string> memberSelector;
    // Expected schema for the decoded payload. CatalogSourceType::Unknown means
    // no hint, so detection alone decides. A concrete hint never overrides
    // detection: detection stays authoritative, and a detected schema that
    // differs from the hint fails with a schema hint mismatch instead of
    // silently parsing whatever was detected.
    CatalogSourceType schemaHint = CatalogSourceType::Unknown;
};

}  // namespace skygate::ephemeris
