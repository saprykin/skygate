#include "catalog/CatalogParsingUtilities.hpp"

#include <QByteArray>
#include <QStringList>

#include <cmath>
#include <cstddef>

namespace skygate::ephemeris::catalog_parsing {

std::string toUtf8String(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

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

std::optional<double> parseFiniteDouble(const QString& text)
{
    return parseFiniteDouble(QStringView {text});
}

std::optional<double> parsePositiveDouble(const QStringView text)
{
    const auto value = parseFiniteDouble(text);
    if (!value.has_value() || *value <= 0.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parsePositiveDouble(const QString& text)
{
    return parsePositiveDouble(QStringView {text});
}

std::optional<double> parseRightAscensionHours(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    const QStringList parts = text.split(':');
    if (parts.size() != 3) {
        return parseFiniteDouble(text);
    }

    const auto hours = parseFiniteDouble(parts.at(0));
    const auto minutes = parseFiniteDouble(parts.at(1));
    const auto seconds = parseFiniteDouble(parts.at(2));
    if (!hours.has_value() || !minutes.has_value() || !seconds.has_value()) {
        return std::nullopt;
    }

    const double value = *hours + (*minutes / 60.0) + (*seconds / 3600.0);
    if (value < 0.0 || value >= 24.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parseDeclinationDeg(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    const QStringList parts = text.split(':');
    if (parts.size() != 3) {
        return parseFiniteDouble(text);
    }

    const double sign = text.startsWith('-') ? -1.0 : 1.0;
    QString degreesText = parts.at(0);
    degreesText.remove('+');
    degreesText.remove('-');
    const auto degrees = parseFiniteDouble(degreesText);
    const auto arcminutes = parseFiniteDouble(parts.at(1));
    const auto arcseconds = parseFiniteDouble(parts.at(2));
    if (!degrees.has_value() || !arcminutes.has_value() || !arcseconds.has_value()) {
        return std::nullopt;
    }

    const double value = sign * (*degrees + (*arcminutes / 60.0) + (*arcseconds / 3600.0));
    if (value < -90.0 || value > 90.0) {
        return std::nullopt;
    }
    return value;
}

}  // namespace skygate::ephemeris::catalog_parsing
