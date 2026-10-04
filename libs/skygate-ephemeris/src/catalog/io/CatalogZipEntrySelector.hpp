#pragma once

#include "CatalogZipEntrySelection.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

// Applies the deterministic catalog member selection policy to a ZIP archive.
// Without an explicit member selector, the unique candidate that decodes to a
// supported schema is chosen; multiple supported candidates are reported as
// ambiguous rather than silently picking the first CSV. An explicit selector
// must name an existing, readable, non-directory, non-encrypted member.
class CatalogZipEntrySelector final {
public:
    [[nodiscard]] static CatalogZipEntrySelection
    select(std::string_view zipData, const std::optional<std::string>& memberSelector);
};

}  // namespace skygate::ephemeris
