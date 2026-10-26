#include "StellariumLineRefExtractor.hpp"
#include "StellariumHipParser.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <QJsonValue>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>

namespace skygate::ephemeris {

std::vector<ConstellationLineRef> StellariumLineRefExtractor::extract(const QJsonObject& rootObject)
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
            CatalogIdentifier::hipBodyId(static_cast<std::uint64_t>(startHip)),
            CatalogIdentifier::hipBodyId(static_cast<std::uint64_t>(endHip))
        );
    };

    const auto appendPolylines = [&appendSegment](const std::vector<std::vector<int>>& polylines) {
        for (const auto& polyline : polylines) {
            for (std::size_t index = 1; index < polyline.size(); ++index) {
                appendSegment(polyline[index - 1], polyline[index]);
            }
        }
    };

    // An object entry may keep its polylines under `lines`/`line` or inside a
    // nested payload; the shared collector walks either shape.
    const auto collectFromEntry = [&appendPolylines](const QJsonValue& entry) {
        std::vector<std::vector<int>> polylines;
        StellariumHipParser::collectHipPolylines(entry, polylines);
        appendPolylines(polylines);
    };

    StellariumHipParser::forEachConstellationEntry(
        rootObject, [&collectFromEntry](const QJsonValue& entry, const QString&) { collectFromEntry(entry); }
    );

    return lineRefs;
}

}  // namespace skygate::ephemeris
