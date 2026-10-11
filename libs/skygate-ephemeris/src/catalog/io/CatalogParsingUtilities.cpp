#include "CatalogParsingUtilities.hpp"
#include "EquatorialCoordinate.hpp"
#include "NumericParsingUtilities.hpp"

#include <QByteArray>
#include <QStringList>

#include <cstddef>

namespace skygate::ephemeris {
namespace {

// Splits decimal or sexagesimal coordinate text and composes the value without
// range checks, which differ between right ascension and declination.
std::optional<double> parseSexagesimal(const QString& text, const bool allowsNegativeDegrees)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    const QStringList parts = trimmed.split(':');
    if (parts.size() != 3) {
        return CatalogParsingUtilities::parseFiniteDouble(trimmed);
    }

    const double sign = allowsNegativeDegrees && trimmed.startsWith('-') ? -1.0 : 1.0;
    QString degreesText = parts.at(0);
    if (allowsNegativeDegrees) {
        degreesText.remove('+');
        degreesText.remove('-');
    }

    const auto degrees = CatalogParsingUtilities::parseFiniteDouble(degreesText);
    const auto arcminutes = CatalogParsingUtilities::parseFiniteDouble(parts.at(1));
    const auto arcseconds = CatalogParsingUtilities::parseFiniteDouble(parts.at(2));
    if (!degrees.has_value() || !arcminutes.has_value() || !arcseconds.has_value()) {
        return std::nullopt;
    }

    return sign * (*degrees + (*arcminutes / 60.0) + (*arcseconds / 3600.0));
}

}  // namespace

std::string CatalogParsingUtilities::toUtf8String(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

std::optional<double> CatalogParsingUtilities::parseFiniteDouble(const QStringView text)
{
    QString trimmed = text.trimmed().toString();
    if (trimmed.startsWith(QLatin1Char('+'))) {
        trimmed.removeFirst();
    }

    return NumericParsingUtilities::parseFiniteDouble(toUtf8String(trimmed));
}

std::optional<double> CatalogParsingUtilities::parseFiniteDouble(const QString& text)
{
    return parseFiniteDouble(QStringView{text});
}

std::optional<double> CatalogParsingUtilities::parsePositiveDouble(const QStringView text)
{
    const auto value = parseFiniteDouble(text);
    if (!value.has_value() || *value <= 0.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> CatalogParsingUtilities::parsePositiveDouble(const QString& text)
{
    return parsePositiveDouble(QStringView{text});
}

std::optional<double> CatalogParsingUtilities::parseNonNegativeDouble(const QStringView text)
{
    const auto value = parseFiniteDouble(text);
    if (!value.has_value() || *value < 0.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> CatalogParsingUtilities::parseNonNegativeDouble(const QString& text)
{
    return parseNonNegativeDouble(QStringView{text});
}

std::optional<double> CatalogParsingUtilities::parseRightAscensionHours(QString text)
{
    const auto value = parseSexagesimal(text, false);
    if (!value.has_value() || *value < skygate::core::EquatorialCoordinate::kRightAscensionMinHours
        || *value >= skygate::core::EquatorialCoordinate::kRightAscensionMaxHours) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> CatalogParsingUtilities::parseDeclinationDeg(QString text)
{
    const auto value = parseSexagesimal(text, true);
    if (!value.has_value() || *value < skygate::core::EquatorialCoordinate::kDeclinationMinDeg
        || *value > skygate::core::EquatorialCoordinate::kDeclinationMaxDeg) {
        return std::nullopt;
    }
    return value;
}

}  // namespace skygate::ephemeris
