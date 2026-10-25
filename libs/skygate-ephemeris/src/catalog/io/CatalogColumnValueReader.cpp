#include "CatalogColumnValueReader.hpp"
#include "CatalogParsingUtilities.hpp"
#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"

#include <QString>

#include <cstdint>
#include <optional>

namespace skygate::ephemeris {
namespace {

// Decodes one column and applies `parse` to its text. An empty column is an
// absent value, so the domain checks never see the absent marker.
template <typename Parse>
[[nodiscard]] std::optional<double> parseColumn(const DelimitedCatalogRow& row, const QString& column, Parse parse)
{
    const QString text = row.decodeColumn(column);
    if (text.isEmpty()) {
        return std::nullopt;
    }

    return parse(text);
}

}  // namespace

std::optional<double> CatalogColumnValueReader::finiteDouble(const DelimitedCatalogRow& row, const QString& column)
{
    return parseColumn(row, column, [](const QString& text) {
        return CatalogParsingUtilities::parseFiniteDouble(text);
    });
}

std::optional<double> CatalogColumnValueReader::positiveDouble(const DelimitedCatalogRow& row, const QString& column)
{
    return parseColumn(row, column, [](const QString& text) {
        return CatalogParsingUtilities::parsePositiveDouble(text);
    });
}

std::optional<double> CatalogColumnValueReader::nonNegativeDouble(const DelimitedCatalogRow& row, const QString& column)
{
    return parseColumn(row, column, [](const QString& text) {
        return CatalogParsingUtilities::parseNonNegativeDouble(text);
    });
}

std::optional<double>
CatalogColumnValueReader::rightAscensionHours(const DelimitedCatalogRow& row, const QString& column)
{
    return parseColumn(row, column, [](const QString& text) {
        return CatalogParsingUtilities::parseRightAscensionHours(text);
    });
}

std::optional<double> CatalogColumnValueReader::declinationDeg(const DelimitedCatalogRow& row, const QString& column)
{
    return parseColumn(row, column, [](const QString& text) {
        return CatalogParsingUtilities::parseDeclinationDeg(text);
    });
}

std::optional<std::uint64_t> CatalogColumnValueReader::uint64(const DelimitedCatalogRow& row, const QString& column)
{
    return parseColumn(row, column, [](const QString& text) {
        return NumericParsingUtilities::parseUint64(StringUtilities::toUtf8(text));
    });
}

}  // namespace skygate::ephemeris
