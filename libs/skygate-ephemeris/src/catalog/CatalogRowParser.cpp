#include "catalog/CatalogRowParser.hpp"

#include <QString>
#include <QStringList>
#include <QStringView>

#include <cmath>
#include <optional>

namespace skygate::ephemeris {
namespace {

std::optional<double> parseFiniteDouble(const QStringView text)
{
    const QString trimmed = text.trimmed().toString();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool isDouble = false;
    const double value = trimmed.toDouble(&isDouble);
    if (!isDouble || !std::isfinite(value)) {
        return std::nullopt;
    }

    return value;
}

std::optional<CelestialBodyType> parseBodyType(const QStringView typeText)
{
    const QString normalized = typeText.trimmed().toString();
    if (normalized == "Star") {
        return CelestialBodyType::Star;
    }
    if (normalized == "Planet") {
        return CelestialBodyType::Planet;
    }
    if (normalized == "Moon") {
        return CelestialBodyType::Moon;
    }
    if (normalized == "Sun") {
        return CelestialBodyType::Sun;
    }
    if (normalized == "Constellation") {
        return CelestialBodyType::Constellation;
    }
    return std::nullopt;
}

std::optional<CelestialBody> parseBodyRow(const QStringView rowText)
{
    const QStringList columns = rowText.toString().split('|', Qt::KeepEmptyParts);
    if (columns.size() < 4) {
        return std::nullopt;
    }

    const QString id = columns.at(0).trimmed();
    const QString displayName = columns.at(1).trimmed();
    const auto type = parseBodyType(QStringView {columns.at(2)});
    const auto visualMagnitude = parseFiniteDouble(QStringView {columns.at(3)});
    if (id.isEmpty() || displayName.isEmpty() || !type.has_value() || !visualMagnitude.has_value()) {
        return std::nullopt;
    }

    CelestialBody body;
    body.id = id.toStdString();
    body.displayName = displayName.toStdString();
    body.type = *type;
    body.visualMagnitude = *visualMagnitude;

    if (columns.size() >= 6) {
        const auto rightAscensionHours = parseFiniteDouble(QStringView {columns.at(4)});
        const auto declinationDeg = parseFiniteDouble(QStringView {columns.at(5)});
        if (rightAscensionHours.has_value() && declinationDeg.has_value()) {
            body.fixedEquatorial = core::EquatorialCoordinate {
                .rightAscensionHours = *rightAscensionHours,
                .declinationDeg = *declinationDeg
            };
        }
    }

    return body;
}

}  // namespace

CatalogBodyParseResult CatalogRowParser::parse(const std::string_view catalogRows) const
{
    CatalogBodyParseResult result;
    if (catalogRows.empty()) {
        result.errorCode = CatalogLoadErrorCode::EmptyInput;
        result.errorDetail = "Catalog rows payload is empty.";
        return result;
    }

    const QString rows = QString::fromUtf8(
        catalogRows.data(),
        static_cast<qsizetype>(catalogRows.size())
    );

    const QStringList lines = rows.split('\n', Qt::KeepEmptyParts);
    result.bodies.reserve(static_cast<std::size_t>(lines.size()));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        ++result.diagnostics.processedRowCount;

        if (const auto parsedBody = parseBodyRow(QStringView {line}); parsedBody.has_value()) {
            result.bodies.push_back(*parsedBody);
        }
    }

    if (result.bodies.empty()) {
        result.errorCode = CatalogLoadErrorCode::InvalidRows;
        result.errorDetail = "Catalog rows payload does not contain any valid rows.";
        return result;
    }

    result.diagnostics.parsedBodyCount = result.bodies.size();
    return result;
}

}  // namespace skygate::ephemeris
