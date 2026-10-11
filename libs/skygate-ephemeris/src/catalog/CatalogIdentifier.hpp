#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

// One namespaced external catalog identifier, for example { "hip", "32349" },
// { "hyg", "1" }, { "messier", "031" }, or { "ngc", "224" }.
//
// The namespace separates identifiers whose numeric values would otherwise
// collide: HIP 32349 and HYG 32349 are different source records. Values are
// normalized per namespace (ASCII case is folded, leading zeros are stripped;
// Messier numbers are padded to three digits) so equivalent spellings such as
// NGC 1234A and ngc1234a compare equal within a namespace, while equal
// numerics in different namespaces never collide.
struct CatalogIdentifier {
    std::string namespaceName;
    std::string value;

    [[nodiscard]] static CatalogIdentifier make(std::string namespaceName, std::string value);
    [[nodiscard]] static std::string normalizeValue(std::string_view namespaceName, std::string_view value);
    // Canonical body id of the form "<namespace>_<normalized value>", so a
    // record's public id and its external identifier always agree.
    [[nodiscard]] static std::string bodyId(std::string namespaceName, std::string value);
    // The deep-sky designation namespaces whose canonical body ids, ngc_<n>,
    // ic_<n>, and messier_<nnn>, are recognized designations.
    [[nodiscard]] static bool isDeepSkyDesignationNamespace(std::string_view namespaceName) noexcept;
    // The designation a canonical deep-sky body id was derived from, matched
    // ASCII case-insensitively like every other identity key.
    [[nodiscard]] static std::optional<CatalogIdentifier> designationFromCanonicalId(std::string_view canonicalId);
    // Appends `identifier` unless it is empty or already present.
    static bool appendUnique(std::vector<CatalogIdentifier>& identifiers, CatalogIdentifier identifier);
    // Drops leading zeros from a catalog number, keeping a single zero for a
    // zero value.
    [[nodiscard]] static std::string stripLeadingZeros(std::string value);

    // Stable collision-free key of the form "<namespace>:<value>".
    [[nodiscard]] std::string key() const;
    [[nodiscard]] bool empty() const noexcept;

    bool operator==(const CatalogIdentifier& other) const = default;
    bool operator!=(const CatalogIdentifier& other) const = default;
    [[nodiscard]] bool operator<(const CatalogIdentifier& other) const;
};

}  // namespace skygate::ephemeris
