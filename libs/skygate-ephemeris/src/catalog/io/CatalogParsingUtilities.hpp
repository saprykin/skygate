#pragma once

#include <QString>
#include <QStringView>

#include <optional>
#include <string>

namespace skygate::ephemeris {

// Catalog field parsing: coordinate text, screened numeric domains, and the
// absences of the value model. Text normalization and the UTF-8 payload bridge
// live in StringUtilities.
class CatalogParsingUtilities final {
public:
    // Forwards to StringUtilities::toUtf8, which owns the one UTF-8 bridge of a
    // decoded payload text.
    [[nodiscard]] static std::string toUtf8String(const QString& text);
    [[nodiscard]] static std::optional<double> parseFiniteDouble(QStringView text);
    [[nodiscard]] static std::optional<double> parseFiniteDouble(const QString& text);
    [[nodiscard]] static std::optional<double> parsePositiveDouble(QStringView text);
    [[nodiscard]] static std::optional<double> parsePositiveDouble(const QString& text);
    [[nodiscard]] static std::optional<double> parseNonNegativeDouble(QStringView text);
    [[nodiscard]] static std::optional<double> parseNonNegativeDouble(const QString& text);
    // Screens a value that already exists numerically with the same domains the
    // text parsers apply, so tabulated data cannot admit a value parsed text
    // would reject.
    [[nodiscard]] static std::optional<double> screenedPositiveValue(double value);
    [[nodiscard]] static std::optional<double> screenedNonNegativeValue(double value);
    [[nodiscard]] static std::optional<double> parseRightAscensionHours(QString text);
    [[nodiscard]] static std::optional<double> parseDeclinationDeg(QString text);
};

}  // namespace skygate::ephemeris
