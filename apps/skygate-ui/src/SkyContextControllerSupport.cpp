#include "SkyContextControllerSupport.hpp"

#include <Qt>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QTimeZone>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
struct ConstellationLineRef {
    std::string_view startId;
    std::string_view endId;
};

constexpr std::array<ConstellationLineRef, 57> kDefaultConstellationLineRefs = {{
    // Bundled fallback.
    {.startId = "sirius", .endId = "procyon"},
    {.startId = "procyon", .endId = "betelgeuse"},
    {.startId = "betelgeuse", .endId = "rigel"},
    {.startId = "rigel", .endId = "sirius"},
    {.startId = "betelgeuse", .endId = "capella"},
    {.startId = "capella", .endId = "vega"},
    {.startId = "vega", .endId = "arcturus"},
    // Orion.
    {.startId = "hip_27989", .endId = "hip_25336"},
    {.startId = "hip_27989", .endId = "hip_26727"},
    {.startId = "hip_25336", .endId = "hip_25930"},
    {.startId = "hip_25930", .endId = "hip_26311"},
    {.startId = "hip_26311", .endId = "hip_26727"},
    {.startId = "hip_26727", .endId = "hip_27366"},
    {.startId = "hip_27366", .endId = "hip_24436"},
    {.startId = "hip_24436", .endId = "hip_25930"},
    {.startId = "hip_26207", .endId = "hip_25930"},
    {.startId = "hip_26207", .endId = "hip_27989"},
    // Ursa Major.
    {.startId = "hip_67301", .endId = "hip_65378"},
    {.startId = "hip_65378", .endId = "hip_62956"},
    {.startId = "hip_62956", .endId = "hip_59774"},
    {.startId = "hip_59774", .endId = "hip_54061"},
    {.startId = "hip_54061", .endId = "hip_53910"},
    {.startId = "hip_53910", .endId = "hip_58001"},
    // Ursa Minor.
    {.startId = "hip_11767", .endId = "hip_79822"},
    {.startId = "hip_79822", .endId = "hip_77055"},
    {.startId = "hip_77055", .endId = "hip_75097"},
    {.startId = "hip_75097", .endId = "hip_72607"},
    {.startId = "hip_72607", .endId = "hip_85822"},
    {.startId = "hip_85822", .endId = "hip_82080"},
    {.startId = "hip_82080", .endId = "hip_11767"},
    // Cassiopeia.
    {.startId = "hip_746", .endId = "hip_3179"},
    {.startId = "hip_3179", .endId = "hip_4427"},
    {.startId = "hip_4427", .endId = "hip_6686"},
    {.startId = "hip_6686", .endId = "hip_8886"},
    // Cygnus.
    {.startId = "hip_102098", .endId = "hip_100453"},
    {.startId = "hip_100453", .endId = "hip_95947"},
    {.startId = "hip_100453", .endId = "hip_98110"},
    {.startId = "hip_100453", .endId = "hip_97165"},
    {.startId = "hip_98110", .endId = "hip_95947"},
    // Taurus.
    {.startId = "hip_20889", .endId = "hip_21421"},
    {.startId = "hip_20889", .endId = "hip_25428"},
    {.startId = "hip_21421", .endId = "hip_25428"},
    // Gemini.
    {.startId = "hip_31681", .endId = "hip_34088"},
    {.startId = "hip_34088", .endId = "hip_35550"},
    {.startId = "hip_37826", .endId = "hip_34088"},
    {.startId = "hip_34088", .endId = "hip_32362"},
    // Leo.
    {.startId = "hip_49669", .endId = "hip_54872"},
    {.startId = "hip_54872", .endId = "hip_57632"},
    {.startId = "hip_54872", .endId = "hip_50583"},
    // Andromeda.
    {.startId = "hip_677", .endId = "hip_3092"},
    {.startId = "hip_3092", .endId = "hip_5447"},
    {.startId = "hip_5447", .endId = "hip_9640"},
    // Scorpius.
    {.startId = "hip_78265", .endId = "hip_78401"},
    {.startId = "hip_78401", .endId = "hip_78820"},
    {.startId = "hip_78820", .endId = "hip_80763"},
    {.startId = "hip_80763", .endId = "hip_85927"},
    {.startId = "hip_85927", .endId = "hip_86228"},
}};

std::vector<std::string_view> splitWhitespaceColumns(std::string_view line)
{
    std::vector<std::string_view> columns;
    std::size_t index = 0;
    while (index < line.size()) {
        while (
            index < line.size()
            && std::isspace(static_cast<unsigned char>(line[index]))
        ) {
            ++index;
        }
        if (index >= line.size()) {
            break;
        }

        const std::size_t tokenStart = index;
        while (
            index < line.size()
            && !std::isspace(static_cast<unsigned char>(line[index]))
        ) {
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

QString bodyTypeToString(const skygate::ephemeris::CelestialBodyType type)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Star:
        return "Star";
    case skygate::ephemeris::CelestialBodyType::Planet:
        return "Planet";
    case skygate::ephemeris::CelestialBodyType::Moon:
        return "Moon";
    case skygate::ephemeris::CelestialBodyType::Sun:
        return "Sun";
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return "Constellation";
    }

    return "Star";
}

QString sanitizeCatalogField(QString value)
{
    value.replace('|', '/');
    value.replace('\n', ' ');
    value.replace('\r', ' ');
    return value.trimmed();
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
} // namespace

namespace skygate::ui::internal {
std::vector<std::pair<std::string, std::string>> defaultConstellationLineRefs()
{
    std::vector<std::pair<std::string, std::string>> lineRefs;
    lineRefs.reserve(kDefaultConstellationLineRefs.size());
    for (const auto& lineRef : kDefaultConstellationLineRefs) {
        lineRefs.emplace_back(std::string(lineRef.startId), std::string(lineRef.endId));
    }
    return lineRefs;
}

std::vector<std::pair<std::string, std::vector<std::string>>> defaultConstellationLabelRefs()
{
    return {
        {"Orion", {"hip_27989", "hip_25336", "hip_25930", "hip_26311", "hip_26727", "hip_24436"}},
        {"Ursa Major", {"hip_67301", "hip_65378", "hip_62956", "hip_59774", "hip_54061", "hip_53910", "hip_58001"}},
        {"Ursa Minor", {"hip_11767", "hip_79822", "hip_77055", "hip_75097", "hip_72607", "hip_85822", "hip_82080"}},
        {"Cassiopeia", {"hip_746", "hip_3179", "hip_4427", "hip_6686", "hip_8886"}},
        {"Cygnus", {"hip_102098", "hip_100453", "hip_95947", "hip_98110", "hip_97165"}},
        {"Taurus", {"hip_20889", "hip_21421", "hip_25428"}},
        {"Gemini", {"hip_31681", "hip_34088", "hip_35550", "hip_37826", "hip_32362"}},
        {"Leo", {"hip_49669", "hip_54872", "hip_57632", "hip_50583"}},
        {"Andromeda", {"hip_677", "hip_3092", "hip_5447", "hip_9640"}},
        {"Scorpius", {"hip_78265", "hip_78401", "hip_78820", "hip_80763", "hip_85927", "hip_86228"}},
    };
}

std::vector<std::pair<std::string, std::string>> parseStellariumConstellationLineRefs(
    const std::string_view constellationLineRows
)
{
    std::vector<std::pair<std::string, std::string>> lineRefs;
    std::unordered_set<std::string> dedupKeys;

    std::size_t cursor = 0;
    while (cursor < constellationLineRows.size()) {
        const std::size_t newline = constellationLineRows.find('\n', cursor);
        const std::size_t lineEnd =
            newline == std::string_view::npos ? constellationLineRows.size() : newline;
        std::string_view line = constellationLineRows.substr(cursor, lineEnd - cursor);

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

std::vector<std::pair<std::string, std::string>> parseStellariumIndexJsonConstellationLineRefs(
    const QByteArray& jsonPayload
)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    std::vector<std::pair<std::string, std::string>> lineRefs;
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
    const auto appendPolylines = [&](const std::vector<std::vector<int>>& polylines) {
        for (const auto& polyline : polylines) {
            for (std::size_t index = 1; index < polyline.size(); ++index) {
                appendSegment(polyline[index - 1], polyline[index]);
            }
        }
    };

    const auto collectFromEntry = [&](const QJsonValue& entry) {
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

    const QJsonValue constellationsValue = document.object().value("constellations");
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

std::vector<std::pair<std::string, std::vector<std::string>>> parseStellariumIndexJsonConstellationLabelRefs(
    const QByteArray& jsonPayload
)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    std::vector<std::pair<std::string, std::vector<std::string>>> labelRefs;
    std::unordered_map<std::string, std::size_t> indexByLabelKey;

    const auto addLabelEntry = [&](QString labelText, const std::vector<std::vector<int>>& polylines) {
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

    const auto collectEntry = [&](const QJsonValue& entry, const QString& fallbackId) {
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

    const QJsonValue constellationsValue = document.object().value("constellations");
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

QString formatCoordinate(const double value)
{
    return QString::number(value, 'f', 6);
}

QString formatElevation(const double value)
{
    return QString::number(value, 'f', 1);
}

QString projectionTypeToString(const skygate::core::ProjectionType projectionType)
{
    switch (projectionType) {
    case skygate::core::ProjectionType::Stereographic:
        return "Stereographic";
    case skygate::core::ProjectionType::AzimuthalEquidistant:
        return "AzimuthalEquidistant";
    case skygate::core::ProjectionType::Perspective:
        return "Perspective";
    }

    return "Unknown";
}

std::optional<skygate::core::ProjectionType> projectionTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "stereographic") {
        return skygate::core::ProjectionType::Stereographic;
    }

    if (normalized == "azimuthalequidistant") {
        return skygate::core::ProjectionType::AzimuthalEquidistant;
    }

    if (normalized == "perspective") {
        return skygate::core::ProjectionType::Perspective;
    }

    return std::nullopt;
}

QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(utcTime.time_since_epoch());
    return QDateTime::fromSecsSinceEpoch(seconds.count(), QTimeZone::UTC);
}

skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcTime)
{
    return skygate::core::UtcTimePoint(std::chrono::seconds(utcTime.toSecsSinceEpoch()));
}

QString settingsKey(const QString& name)
{
    return QString("skyContext/%1").arg(name);
}

QString defaultCatalogCachePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {};
    }

    return QDir(appDataPath).filePath(QString::fromUtf8(kCatalogCacheFileName));
}

QByteArray serializeCatalogRows(const std::vector<skygate::ephemeris::CelestialBody>& bodies)
{
    QByteArray rows;
    rows.reserve(static_cast<int>(bodies.size() * 64));
    for (const auto& body : bodies) {
        const QString id = sanitizeCatalogField(QString::fromStdString(body.id));
        const QString displayName = sanitizeCatalogField(QString::fromStdString(body.displayName));
        if (id.isEmpty() || displayName.isEmpty()) {
            continue;
        }

        rows.append(id.toUtf8());
        rows.append('|');
        rows.append(displayName.toUtf8());
        rows.append('|');
        rows.append(bodyTypeToString(body.type).toUtf8());
        rows.append('|');
        rows.append(QByteArray::number(body.visualMagnitude, 'g', 17));
        if (body.fixedEquatorial.has_value()) {
            rows.append('|');
            rows.append(QByteArray::number(body.fixedEquatorial->rightAscensionHours, 'g', 17));
            rows.append('|');
            rows.append(QByteArray::number(body.fixedEquatorial->declinationDeg, 'g', 17));
        }
        rows.append('\n');
    }
    return rows;
}

QByteArray serializeConstellationLineRows(
    const std::vector<std::pair<std::string, std::string>>& lineRefs
)
{
    QByteArray rows;
    rows.reserve(static_cast<int>(lineRefs.size() * 24));
    for (const auto& lineRef : lineRefs) {
        if (lineRef.first.empty() || lineRef.second.empty()) {
            continue;
        }

        rows.append(QByteArray::fromStdString(lineRef.first));
        rows.append('|');
        rows.append(QByteArray::fromStdString(lineRef.second));
        rows.append('\n');
    }
    return rows;
}

std::vector<std::pair<std::string, std::string>> parseConstellationLineRows(
    const std::string_view rows
)
{
    std::vector<std::pair<std::string, std::string>> lineRefs;
    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd =
            newline == std::string_view::npos ? rows.size() : newline;
        const std::string_view line = rows.substr(cursor, lineEnd - cursor);

        if (!line.empty()) {
            const std::size_t delimiter = line.find('|');
            if (delimiter != std::string_view::npos) {
                const std::string_view startId = line.substr(0, delimiter);
                const std::string_view endId = line.substr(delimiter + 1);
                if (!startId.empty() && !endId.empty()) {
                    lineRefs.emplace_back(std::string(startId), std::string(endId));
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

QByteArray serializeConstellationLabelRows(
    const std::vector<std::pair<std::string, std::vector<std::string>>>& labelRefs
)
{
    QByteArray rows;
    rows.reserve(static_cast<int>(labelRefs.size() * 48));
    for (const auto& labelRef : labelRefs) {
        if (labelRef.first.empty() || labelRef.second.empty()) {
            continue;
        }
        const int rowStartSize = rows.size();

        QString sanitizedLabel = QString::fromStdString(labelRef.first);
        sanitizedLabel.replace('|', '/');
        sanitizedLabel.replace('\n', ' ');
        sanitizedLabel.replace('\r', ' ');
        sanitizedLabel = sanitizedLabel.trimmed();
        if (sanitizedLabel.isEmpty()) {
            continue;
        }

        rows.append(sanitizedLabel.toUtf8());
        rows.append('|');
        bool hasAnyHip = false;
        for (const std::string& hipId : labelRef.second) {
            if (hipId.empty()) {
                continue;
            }
            if (hasAnyHip) {
                rows.append(',');
            }
            rows.append(QByteArray::fromStdString(hipId));
            hasAnyHip = true;
        }
        if (!hasAnyHip) {
            rows.truncate(rowStartSize);
            continue;
        }
        rows.append('\n');
    }

    return rows;
}

std::vector<std::pair<std::string, std::vector<std::string>>> parseConstellationLabelRows(
    const std::string_view rows
)
{
    std::vector<std::pair<std::string, std::vector<std::string>>> labelRefs;
    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? rows.size() : newline;
        const std::string_view line = rows.substr(cursor, lineEnd - cursor);

        if (!line.empty()) {
            const std::size_t delimiter = line.find('|');
            if (delimiter != std::string_view::npos) {
                const std::string_view label = line.substr(0, delimiter);
                const std::string_view hipList = line.substr(delimiter + 1);
                if (!label.empty() && !hipList.empty()) {
                    std::vector<std::string> hipIds;
                    std::size_t hipCursor = 0;
                    while (hipCursor < hipList.size()) {
                        const std::size_t comma = hipList.find(',', hipCursor);
                        const std::size_t hipEnd =
                            comma == std::string_view::npos ? hipList.size() : comma;
                        const std::string_view hipId = hipList.substr(hipCursor, hipEnd - hipCursor);
                        if (!hipId.empty()) {
                            hipIds.emplace_back(hipId);
                        }
                        if (comma == std::string_view::npos) {
                            break;
                        }
                        hipCursor = comma + 1;
                    }

                    if (!hipIds.empty()) {
                        labelRefs.emplace_back(std::string(label), std::move(hipIds));
                    }
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }

    return labelRefs;
}

double pointSizeForMagnitude(const double magnitude)
{
    const double normalizedBrightness = std::clamp(1.0 - ((magnitude + 1.5) / 8.0), 0.2, 1.0);
    return 1.8 + normalizedBrightness * 5.0;
}

QColor colorForBodyType(const skygate::ephemeris::CelestialBodyType type)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return QColor(255, 214, 128, 230);
    case skygate::ephemeris::CelestialBodyType::Moon:
        return QColor(162, 245, 255, 235);
    case skygate::ephemeris::CelestialBodyType::Planet:
        return QColor(255, 188, 140, 220);
    case skygate::ephemeris::CelestialBodyType::Star:
        return QColor(188, 214, 255, 210);
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return QColor(160, 244, 200, 205);
    }

    return QColor(220, 220, 240, 200);
}

QColor constellationLineColor()
{
    return QColor(146, 205, 255, 132);
}

std::string_view hipSuffix(std::string_view value)
{
    constexpr std::string_view kHipPrefix = "hip_";
    if (!value.starts_with(kHipPrefix)) {
        return {};
    }
    return value.substr(kHipPrefix.size());
}

double normalizeAzimuthDeg(const double azimuthDeg)
{
    const double normalized = std::fmod(azimuthDeg, 360.0);
    if (normalized < 0.0) {
        return normalized + 360.0;
    }

    return normalized;
}

double clampAltitudeDeg(const double altitudeDeg)
{
    return std::clamp(altitudeDeg, kViewAltitudeMinDeg, kViewAltitudeMaxDeg);
}

double clampFieldOfViewDeg(const double fieldOfViewDeg)
{
    return std::clamp(fieldOfViewDeg, kViewportFieldOfViewMinDeg, kViewportFieldOfViewMaxDeg);
}

skygate::core::ProjectionParams buildProjectionParams(
    const double viewportWidth,
    const double viewportHeight,
    const double centerAltitudeDeg,
    const double centerAzimuthDeg,
    const double fieldOfViewDeg
)
{
    skygate::core::ProjectionParams projectionParams;
    projectionParams.center = {
        .altitudeDeg = centerAltitudeDeg,
        .azimuthDeg = centerAzimuthDeg
    };
    projectionParams.fovDeg = fieldOfViewDeg;
    projectionParams.rollDeg = 0.0;
    projectionParams.viewportWidth = viewportWidth;
    projectionParams.viewportHeight = viewportHeight;
    return projectionParams;
}
} // namespace skygate::ui::internal
