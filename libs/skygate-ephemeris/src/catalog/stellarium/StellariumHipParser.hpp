#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace skygate::ephemeris {

class StellariumHipParser final {
public:
    [[nodiscard]] static std::optional<int> parseStrictHipText(QString text);
    [[nodiscard]] static std::optional<int> parseHipIdentifier(const QJsonValue& value);
    [[nodiscard]] static std::optional<std::vector<int>> parseHipPolyline(const QJsonArray& array);
    static void collectHipPolylines(const QJsonValue& value, std::vector<std::vector<int>>& polylines);

    // Visits every constellation entry of a Stellarium payload: each element of
    // a `constellations` array, or, for a `constellations` object, its
    // `lines`/`line` members or every member with its key as the fallback id.
    // Extractors keep their own label and de-duplication rules; only the shape
    // probing lives here.
    static void forEachConstellationEntry(
        const QJsonObject& rootObject, const std::function<void(const QJsonValue&, const QString&)>& visit
    );
    // Number of entries the `constellations` array carries; a payload without
    // a constellations array has none.
    [[nodiscard]] static std::size_t constellationArrayEntryCount(const QJsonObject& rootObject);
};

}  // namespace skygate::ephemeris
