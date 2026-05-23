#pragma once

#include <QString>
#include <QStringView>

#include <optional>
#include <string>

namespace skygate::ephemeris {

class CatalogParsingUtilities final {
public:
    [[nodiscard]] static std::string toUtf8String(const QString& text);
    [[nodiscard]] static std::optional<double> parseFiniteDouble(QStringView text);
    [[nodiscard]] static std::optional<double> parseFiniteDouble(const QString& text);
    [[nodiscard]] static std::optional<double> parsePositiveDouble(QStringView text);
    [[nodiscard]] static std::optional<double> parsePositiveDouble(const QString& text);
    [[nodiscard]] static std::optional<double> parseNonNegativeDouble(QStringView text);
    [[nodiscard]] static std::optional<double> parseNonNegativeDouble(const QString& text);
    [[nodiscard]] static std::optional<double> parseRightAscensionHours(QString text);
    [[nodiscard]] static std::optional<double> parseDeclinationDeg(QString text);
};

}  // namespace skygate::ephemeris
