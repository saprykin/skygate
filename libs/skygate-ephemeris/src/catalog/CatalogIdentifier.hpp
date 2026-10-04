#pragma once

#include <string>
#include <string_view>

namespace skygate::ephemeris {

// One namespaced external catalog identifier, for example { "hip", "32349" },
// { "hyg", "1" }, { "messier", "031" }, or { "ngc", "224" }.
//
// The namespace separates identifiers whose numeric values would otherwise
// collide: HIP 32349 and HYG 32349 are different source records. Values are
// normalized per namespace (leading zeros are stripped; Messier numbers are
// padded to three digits) so equivalent spellings compare equal within a
// namespace, while equal numerics in different namespaces never collide.
struct CatalogIdentifier {
    std::string namespaceName;
    std::string value;

    [[nodiscard]] static CatalogIdentifier make(std::string namespaceName, std::string value);
    [[nodiscard]] static std::string normalizeValue(std::string_view namespaceName, std::string_view value);

    // Stable collision-free key of the form "<namespace>:<value>".
    [[nodiscard]] std::string key() const;
    [[nodiscard]] bool empty() const noexcept;

    bool operator==(const CatalogIdentifier& other) const = default;
    bool operator!=(const CatalogIdentifier& other) const = default;
    [[nodiscard]] bool operator<(const CatalogIdentifier& other) const;
};

}  // namespace skygate::ephemeris
