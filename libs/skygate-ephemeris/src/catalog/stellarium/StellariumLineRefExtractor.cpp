#include "catalog/stellarium/StellariumLineRefExtractor.hpp"

#include "catalog/stellarium/StellariumHipParser.hpp"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>

namespace skygate::ephemeris {

std::vector<ConstellationLineRef> StellariumLineRefExtractor::extract(
    const QJsonObject& rootObject
)
{
    std::vector<ConstellationLineRef> lineRefs;
    std::unordered_set<std::string> dedupKeys;

    const auto appendSegment = [&lineRefs, &dedupKeys](const int startHip, const int endHip) {
        if (startHip <= 0 || endHip <= 0 || startHip == endHip) {
            return;
        }

        const int dedupStart = std::min(startHip, endHip);
        const int dedupEnd = std::max(startHip, endHip);
        const std::string dedupKey = std::to_string(dedupStart) + "|" + std::to_string(dedupEnd);
        if (!dedupKeys.insert(dedupKey).second) {
            return;
        }

        lineRefs.emplace_back(
            "hip_" + std::to_string(startHip),
            "hip_" + std::to_string(endHip)
        );
    };

    const auto appendPolylines = [&appendSegment](const std::vector<std::vector<int>>& polylines) {
        for (const auto& polyline : polylines) {
            for (std::size_t index = 1; index < polyline.size(); ++index) {
                appendSegment(polyline[index - 1], polyline[index]);
            }
        }
    };

    const auto collectFromEntry = [&appendPolylines](const QJsonValue& entry) {
        std::vector<std::vector<int>> polylines;
        if (entry.isObject()) {
            const QJsonObject object = entry.toObject();
            const QJsonValue linesValue = object.value("lines");
            const QJsonValue lineValue = object.value("line");
            if (!linesValue.isUndefined()) {
                StellariumHipParser::collectHipPolylines(linesValue, polylines);
            }
            if (!lineValue.isUndefined()) {
                StellariumHipParser::collectHipPolylines(lineValue, polylines);
            }
        } else if (entry.isArray()) {
            StellariumHipParser::collectHipPolylines(entry, polylines);
        }
        appendPolylines(polylines);
    };

    const QJsonValue constellationsValue = rootObject.value("constellations");
    if (constellationsValue.isArray()) {
        for (const QJsonValue& entry : constellationsValue.toArray()) {
            collectFromEntry(entry);
        }
    } else if (constellationsValue.isObject()) {
        const QJsonObject constellationsObject = constellationsValue.toObject();
        const QJsonValue linesValue = constellationsObject.value("lines");
        const QJsonValue lineValue = constellationsObject.value("line");
        if (!linesValue.isUndefined() || !lineValue.isUndefined()) {
            if (!linesValue.isUndefined()) {
                collectFromEntry(linesValue);
            }
            if (!lineValue.isUndefined()) {
                collectFromEntry(lineValue);
            }
        } else {
            for (auto it = constellationsObject.begin(); it != constellationsObject.end(); ++it) {
                collectFromEntry(it.value());
            }
        }
    }

    return lineRefs;
}

}  // namespace skygate::ephemeris
