#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

std::vector<std::string_view> splitWhitespaceColumns(const std::string_view line)
{
    std::vector<std::string_view> columns;
    std::size_t index = 0;
    while (index < line.size()) {
        while (index < line.size() && std::isspace(static_cast<unsigned char>(line[index]))) {
            ++index;
        }
        if (index >= line.size()) {
            break;
        }

        const std::size_t tokenStart = index;
        while (index < line.size() && !std::isspace(static_cast<unsigned char>(line[index]))) {
            ++index;
        }
        columns.push_back(line.substr(tokenStart, index - tokenStart));
    }

    return columns;
}

std::optional<int> parsePositiveInteger(const std::string_view value)
{
    if (value.empty()) {
        return std::nullopt;
    }

    int parsedValue = 0;
    const auto [parseEnd, errorCode] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsedValue
    );
    if (errorCode != std::errc() || parseEnd != value.data() + value.size() || parsedValue <= 0) {
        return std::nullopt;
    }

    return parsedValue;
}

std::size_t inferConstellationCountFromRows(const std::string_view rows)
{
    std::unordered_set<std::string> constellationIds;

    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? rows.size() : newline;
        std::string_view line = rows.substr(cursor, lineEnd - cursor);

        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
            line.remove_prefix(1);
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.remove_suffix(1);
        }

        if (!line.empty() && line.front() != '#') {
            std::size_t firstSplit = 0;
            while (firstSplit < line.size() && !std::isspace(static_cast<unsigned char>(line[firstSplit]))) {
                ++firstSplit;
            }
            const std::string_view constellationId = line.substr(0, firstSplit);

            while (firstSplit < line.size() && std::isspace(static_cast<unsigned char>(line[firstSplit]))) {
                ++firstSplit;
            }
            std::size_t secondSplit = firstSplit;
            while (secondSplit < line.size() && !std::isspace(static_cast<unsigned char>(line[secondSplit]))) {
                ++secondSplit;
            }
            const std::string_view segmentCountText = line.substr(firstSplit, secondSplit - firstSplit);

            if (!constellationId.empty() && parsePositiveInteger(segmentCountText).has_value()) {
                constellationIds.insert(std::string(constellationId));
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }

    return constellationIds.size();
}

bool looksLikeJsonPayload(const std::string_view payload)
{
    for (const char byte : payload) {
        if (std::isspace(static_cast<unsigned char>(byte))) {
            continue;
        }
        return byte == '{' || byte == '[';
    }

    return false;
}

std::optional<int> parseStrictHipText(QString text)
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

std::optional<int> parseHipIdentifierFromJsonValue(const QJsonValue& value)
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

        if (const auto parsed = parseHipIdentifierFromJsonValue(fieldValue); parsed.has_value()) {
            return parsed;
        }
    }

    return std::nullopt;
}

std::optional<std::vector<int>> parseHipPolyline(const QJsonArray& array)
{
    if (array.size() < 2) {
        return std::nullopt;
    }

    std::vector<int> hips;
    hips.reserve(array.size());
    for (const QJsonValue& child : array) {
        const auto hip = parseHipIdentifierFromJsonValue(child);
        if (!hip.has_value()) {
            return std::nullopt;
        }
        hips.push_back(*hip);
    }

    return hips;
}

void collectHipPolylines(const QJsonValue& value, std::vector<std::vector<int>>& polylines)
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

QString prettifyConstellationId(QString id)
{
    id = id.trimmed();
    id.replace('_', ' ');
    id.replace('-', ' ');
    id = id.simplified();
    if (id.isEmpty()) {
        return {};
    }

    const QString lower = id.toLower();
    QString title;
    title.reserve(lower.size());
    bool wordStart = true;
    for (const QChar c : lower) {
        if (c.isSpace()) {
            title.append(c);
            wordStart = true;
            continue;
        }

        title.append(wordStart ? c.toUpper() : c);
        wordStart = false;
    }

    return title;
}

QString constellationLabelFromEntry(const QString& fallbackId, const QJsonObject& entry)
{
    constexpr std::array<const char*, 3> kDirectNameFields = {{
        "name",
        "label",
        "english",
    }};
    for (const char* fieldName : kDirectNameFields) {
        const QJsonValue fieldValue = entry.value(fieldName);
        if (fieldValue.isString()) {
            const QString text = fieldValue.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        }
    }

    const QJsonValue commonNameValue = entry.value("common_name");
    if (commonNameValue.isString()) {
        const QString text = commonNameValue.toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    } else if (commonNameValue.isObject()) {
        const QJsonObject commonNameObject = commonNameValue.toObject();
        constexpr std::array<const char*, 4> kCommonNameFields = {{
            "english",
            "name",
            "native",
            "iau",
        }};
        for (const char* fieldName : kCommonNameFields) {
            const QJsonValue fieldValue = commonNameObject.value(fieldName);
            if (!fieldValue.isString()) {
                continue;
            }
            const QString text = fieldValue.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        }
    }

    const QJsonValue idValue = entry.value("id");
    if (idValue.isString()) {
        const QString prettified = prettifyConstellationId(idValue.toString());
        if (!prettified.isEmpty()) {
            return prettified;
        }
    }

    return prettifyConstellationId(fallbackId);
}

QString normalizeLabelKey(const QString& label)
{
    return label.trimmed().toLower();
}

std::vector<ConstellationLineRef> parseStellariumRowLineRefs(const std::string_view rows)
{
    std::vector<ConstellationLineRef> lineRefs;
    std::unordered_set<std::string> dedupKeys;

    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? rows.size() : newline;
        std::string_view line = rows.substr(cursor, lineEnd - cursor);

        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
            line.remove_prefix(1);
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.remove_suffix(1);
        }

        if (!line.empty() && line.front() != '#') {
            const std::vector<std::string_view> columns = splitWhitespaceColumns(line);
            if (columns.size() >= 4) {
                const auto segmentCount = parsePositiveInteger(columns[1]);
                if (segmentCount.has_value()) {
                    const std::size_t expectedColumnCount =
                        2 + (static_cast<std::size_t>(*segmentCount) * 2U);
                    if (columns.size() >= expectedColumnCount) {
                        for (int segmentIndex = 0; segmentIndex < *segmentCount; ++segmentIndex) {
                            const std::size_t startColumn =
                                2U + (static_cast<std::size_t>(segmentIndex) * 2U);
                            const auto startHip = parsePositiveInteger(columns[startColumn]);
                            const auto endHip = parsePositiveInteger(columns[startColumn + 1U]);
                            if (!startHip.has_value() || !endHip.has_value()) {
                                continue;
                            }

                            std::string startId = "hip_" + std::to_string(*startHip);
                            std::string endId = "hip_" + std::to_string(*endHip);
                            const std::string dedupKey = startId + "|" + endId;
                            if (!dedupKeys.insert(dedupKey).second) {
                                continue;
                            }

                            lineRefs.emplace_back(std::move(startId), std::move(endId));
                        }
                    }
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }

    return lineRefs;
}

std::vector<ConstellationLineRef> parseStellariumJsonLineRefs(const QJsonObject& rootObject)
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
                collectHipPolylines(linesValue, polylines);
            }
            if (!lineValue.isUndefined()) {
                collectHipPolylines(lineValue, polylines);
            }
        } else if (entry.isArray()) {
            collectHipPolylines(entry, polylines);
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

std::vector<ConstellationLabelRef> parseStellariumJsonLabelRefs(const QJsonObject& rootObject)
{
    std::vector<ConstellationLabelRef> labelRefs;
    std::unordered_map<std::string, std::size_t> indexByLabelKey;

    const auto addLabelEntry = [&labelRefs, &indexByLabelKey](QString labelText, const std::vector<std::vector<int>>& polylines) {
        labelText = labelText.trimmed();
        if (labelText.isEmpty()) {
            return;
        }

        std::unordered_set<int> uniqueHipSet;
        std::vector<int> uniqueHips;
        for (const auto& polyline : polylines) {
            for (const int hip : polyline) {
                if (hip > 0 && uniqueHipSet.insert(hip).second) {
                    uniqueHips.push_back(hip);
                }
            }
        }
        if (uniqueHips.size() < 2U) {
            return;
        }
        std::sort(uniqueHips.begin(), uniqueHips.end());

        const QString labelKey = normalizeLabelKey(labelText);
        const std::string labelKeyStd = labelKey.toStdString();
        auto indexIt = indexByLabelKey.find(labelKeyStd);
        if (indexIt == indexByLabelKey.end()) {
            std::vector<std::string> hipIds;
            hipIds.reserve(uniqueHips.size());
            for (const int hip : uniqueHips) {
                hipIds.push_back("hip_" + std::to_string(hip));
            }
            const std::size_t newIndex = labelRefs.size();
            labelRefs.emplace_back(labelText.toStdString(), std::move(hipIds));
            indexByLabelKey.insert({labelKeyStd, newIndex});
            return;
        }

        std::unordered_set<std::string> existingIds(
            labelRefs[indexIt->second].second.begin(),
            labelRefs[indexIt->second].second.end()
        );
        for (const int hip : uniqueHips) {
            const std::string hipId = "hip_" + std::to_string(hip);
            if (existingIds.insert(hipId).second) {
                labelRefs[indexIt->second].second.push_back(hipId);
            }
        }
    };

    const auto collectEntry = [&addLabelEntry](const QJsonValue& entry, const QString& fallbackId) {
        std::vector<std::vector<int>> polylines;
        QString labelText;

        if (entry.isObject()) {
            const QJsonObject object = entry.toObject();
            const QJsonValue linesValue = object.value("lines");
            const QJsonValue lineValue = object.value("line");
            if (!linesValue.isUndefined()) {
                collectHipPolylines(linesValue, polylines);
            }
            if (!lineValue.isUndefined()) {
                collectHipPolylines(lineValue, polylines);
            }
            labelText = constellationLabelFromEntry(fallbackId, object);
        } else if (entry.isArray()) {
            collectHipPolylines(entry, polylines);
            labelText = prettifyConstellationId(fallbackId);
        }

        addLabelEntry(labelText, polylines);
    };

    const QJsonValue constellationsValue = rootObject.value("constellations");
    if (constellationsValue.isArray()) {
        for (const QJsonValue& entry : constellationsValue.toArray()) {
            collectEntry(entry, {});
        }
    } else if (constellationsValue.isObject()) {
        const QJsonObject constellationsObject = constellationsValue.toObject();
        const QJsonValue linesValue = constellationsObject.value("lines");
        const QJsonValue lineValue = constellationsObject.value("line");
        if (!linesValue.isUndefined() || !lineValue.isUndefined()) {
            if (!linesValue.isUndefined()) {
                collectEntry(linesValue, {});
            }
            if (!lineValue.isUndefined()) {
                collectEntry(lineValue, {});
            }
        } else {
            for (auto it = constellationsObject.begin(); it != constellationsObject.end(); ++it) {
                collectEntry(it.value(), it.key());
            }
        }
    }

    return labelRefs;
}

std::size_t inferConstellationCountFromJson(const QJsonObject& rootObject)
{
    const QJsonValue constellationsValue = rootObject.value("constellations");
    if (constellationsValue.isArray()) {
        return static_cast<std::size_t>(constellationsValue.toArray().size());
    }

    return 0;
}

}  // namespace

StellariumConstellationParser::ParseResult StellariumConstellationParser::parse(
    const std::string_view payload
) const
{
    ParseResult result;
    if (payload.empty()) {
        return result;
    }

    result.isJsonPayload = looksLikeJsonPayload(payload);
    if (!result.isJsonPayload) {
        result.lineRefs = parseStellariumRowLineRefs(payload);
        result.constellationCount = inferConstellationCountFromRows(payload);
        return result;
    }

    QJsonParseError parseError;
    const QByteArray payloadBytes(payload.data(), static_cast<qsizetype>(payload.size()));
    const QJsonDocument document = QJsonDocument::fromJson(payloadBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return result;
    }

    const QJsonObject rootObject = document.object();
    result.lineRefs = parseStellariumJsonLineRefs(rootObject);
    result.labelRefs = parseStellariumJsonLabelRefs(rootObject);
    result.constellationCount = inferConstellationCountFromJson(rootObject);
    return result;
}

}  // namespace skygate::ephemeris
