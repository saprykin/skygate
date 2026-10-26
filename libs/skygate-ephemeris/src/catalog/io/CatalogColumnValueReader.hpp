#pragma once

#include "DelimitedCatalogRow.hpp"

#include <QString>

#include <cstdint>
#include <optional>

namespace skygate::ephemeris {

// Typed optional values read from the columns of a decoded catalog row.
//
// Every reader applies the same rule: an empty column is an absent value, any
// other text is handed to the matching field parser (CatalogParsingUtilities for
// coordinates and magnitudes, NumericParsingUtilities for whole numbers). A
// catalog parser therefore names a column and reads one value, instead of
// repeating the decode-empty-parse chain for each field of every row.
class CatalogColumnValueReader final {
public:
    CatalogColumnValueReader() = delete;

    [[nodiscard]] static std::optional<double> finiteDouble(const DelimitedCatalogRow& row, const QString& column);
    [[nodiscard]] static std::optional<double> positiveDouble(const DelimitedCatalogRow& row, const QString& column);
    [[nodiscard]] static std::optional<double> nonNegativeDouble(const DelimitedCatalogRow& row, const QString& column);
    [[nodiscard]] static std::optional<double>
    rightAscensionHours(const DelimitedCatalogRow& row, const QString& column);
    [[nodiscard]] static std::optional<double> declinationDeg(const DelimitedCatalogRow& row, const QString& column);
    // A whole-number catalog identifier, such as a Hipparcos number.
    [[nodiscard]] static std::optional<std::uint64_t> uint64(const DelimitedCatalogRow& row, const QString& column);
};

}  // namespace skygate::ephemeris
