#include "catalog/stellarium/StellariumHipParser.hpp"

#include <QJsonObject>

#include <array>
#include <cmath>
#include <limits>

namespace skygate::ephemeris {

std::optional<int> StellariumHipParser::parseStrictHipText(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    if (text.startsWith("hip_", Qt::CaseInsensitive)) {
        text.remove(0, 4);
    } else if (text.startsWith("hip", Qt::CaseInsensitive)) {
        text.remove(0, 3);
        while (!text.isEmpty() && (text.front().isSpace() || text.front() == '_')) {
            text.remove(0, 1);
        }
    }

    text = text.trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    for (int index = 0; index < text.size(); ++index) {
        if (!text.at(index).isDigit()) {
            return std::nullopt;
        }
    }

    bool isOk = false;
    const int parsedValue = text.toInt(&isOk);
    if (!isOk || parsedValue <= 0) {
        return std::nullopt;
    }

    return parsedValue;
}

std::optional<int> StellariumHipParser::parseHipIdentifier(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double numericValue = value.toDouble();
        if (
            std::isfinite(numericValue)
            && numericValue > 0.0
            && std::floor(numericValue) == numericValue
            && numericValue <= static_cast<double>(std::numeric_limits<int>::max())
        ) {
            return static_cast<int>(numericValue);
        }
        return std::nullopt;
    }

    if (value.isString()) {
        return parseStrictHipText(value.toString());
    }

    if (!value.isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = value.toObject();
    constexpr std::array<const char*, 4> kHipFieldCandidates = {{
        "hip",
        "HIP",
        "id",
        "star",
    }};
    for (const char* fieldName : kHipFieldCandidates) {
        const QJsonValue fieldValue = object.value(fieldName);
        if (fieldValue.isUndefined()) {
            continue;
        }

        if (const auto parsed = parseHipIdentifier(fieldValue); parsed.has_value()) {
            return parsed;
        }
    }

    return std::nullopt;
}

std::optional<std::vector<int>> StellariumHipParser::parseHipPolyline(
    const QJsonArray& array
)
{
    if (array.size() < 2) {
        return std::nullopt;
    }

    std::vector<int> hips;
    hips.reserve(array.size());
    for (const QJsonValue& child : array) {
        const auto hip = parseHipIdentifier(child);
        if (!hip.has_value()) {
            return std::nullopt;
        }
        hips.push_back(*hip);
    }

    return hips;
}

void StellariumHipParser::collectHipPolylines(
    const QJsonValue& value,
    std::vector<std::vector<int>>& polylines
)
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (const auto polyline = parseHipPolyline(array); polyline.has_value()) {
            polylines.push_back(*polyline);
            return;
        }

        for (const QJsonValue& child : array) {
            if (child.isArray() || child.isObject()) {
                collectHipPolylines(child, polylines);
            }
        }
        return;
    }

    if (!value.isObject()) {
        return;
    }

    const QJsonObject object = value.toObject();
    const QJsonValue linesValue = object.value("lines");
    const QJsonValue lineValue = object.value("line");
    if (!linesValue.isUndefined()) {
        collectHipPolylines(linesValue, polylines);
    }
    if (!lineValue.isUndefined()) {
        collectHipPolylines(lineValue, polylines);
    }

    if (!linesValue.isUndefined() || !lineValue.isUndefined()) {
        return;
    }

    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isArray() || it.value().isObject()) {
            collectHipPolylines(it.value(), polylines);
        }
    }
}

}  // namespace skygate::ephemeris
