#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

// Convenience reader of a ZIP payload: selects and inflates its first CSV
// member through the same selection rules the catalog loader applies. Callers
// that parse catalog payloads resolve entries through CatalogZipEntrySelector
// while building their parse request.
class ZipCodec final {
public:
    [[nodiscard]] std::optional<std::string> extractFirstCsvEntry(std::string_view zipData) const;
};

}  // namespace skygate::ephemeris
