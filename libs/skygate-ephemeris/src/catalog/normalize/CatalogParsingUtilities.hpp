#pragma once

#include <QString>
#include <QStringView>

#include <optional>
#include <string>

namespace skygate::ephemeris::catalog_parsing {

[[nodiscard]] std::string toUtf8String(const QString& text);
[[nodiscard]] std::optional<double> parseFiniteDouble(QStringView text);
[[nodiscard]] std::optional<double> parseFiniteDouble(const QString& text);
[[nodiscard]] std::optional<double> parsePositiveDouble(QStringView text);
[[nodiscard]] std::optional<double> parsePositiveDouble(const QString& text);
[[nodiscard]] std::optional<double> parseNonNegativeDouble(QStringView text);
[[nodiscard]] std::optional<double> parseNonNegativeDouble(const QString& text);
[[nodiscard]] std::optional<double> parseRightAscensionHours(QString text);
[[nodiscard]] std::optional<double> parseDeclinationDeg(QString text);

}  // namespace skygate::ephemeris::catalog_parsing
