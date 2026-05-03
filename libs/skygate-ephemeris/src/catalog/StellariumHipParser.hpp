#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <optional>
#include <vector>

namespace skygate::ephemeris {

class StellariumHipParser final {
public:
    [[nodiscard]] static std::optional<int> parseStrictHipText(QString text);
    [[nodiscard]] static std::optional<int> parseHipIdentifier(const QJsonValue& value);
    [[nodiscard]] static std::optional<std::vector<int>> parseHipPolyline(const QJsonArray& array);
    static void collectHipPolylines(
        const QJsonValue& value,
        std::vector<std::vector<int>>& polylines
    );
};

}  // namespace skygate::ephemeris
