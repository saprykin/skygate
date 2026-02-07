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

    const QJsonValue constellationsValue = document.object().value("constellations");
    std::vector<std::pair<std::string, std::string>> lineRefs;
    std::unordered_set<std::string> dedupKeys;

    const auto parseHipIdentifier = [](const QJsonValue& value) -> std::optional<int> {
        if (value.isObject()) {
            const QJsonObject object = value.toObject();
            const QJsonValue hipValue = object.value("hip");
            if (!hipValue.isUndefined()) {
                if (hipValue.isDouble()) {
                    const double numericValue = hipValue.toDouble();
                    if (
                        std::isfinite(numericValue)
                        && numericValue > 0.0
                        && std::floor(numericValue) == numericValue
                    ) {
                        return static_cast<int>(numericValue);
                    }
                } else if (hipValue.isString()) {
                    QString hipText = hipValue.toString().trimmed();
                    bool isOk = false;
                    const int hipNumber = hipText.toInt(&isOk);
                    if (isOk && hipNumber > 0) {
                        return hipNumber;
                    }
                }
            }
            return std::nullopt;
        }

        if (value.isDouble()) {
            const double numericValue = value.toDouble();
            if (
                std::isfinite(numericValue)
                && numericValue > 0.0
                && std::floor(numericValue) == numericValue
            ) {
                return static_cast<int>(numericValue);
            }
            return std::nullopt;
        }

        if (!value.isString()) {
            return std::nullopt;
        }

        QString textValue = value.toString().trimmed();
        if (textValue.startsWith("hip_", Qt::CaseInsensitive)) {
            textValue.remove(0, 4);
        } else if (textValue.startsWith("hip ", Qt::CaseInsensitive)) {
            textValue.remove(0, 4);
        }

        QString digits;
        digits.reserve(textValue.size());
        for (int index = 0; index < textValue.size(); ++index) {
            const QChar c = textValue.at(index);
            if (c.isDigit()) {
                digits.append(c);
            } else if (!digits.isEmpty()) {
                break;
            }
        }

        if (digits.isEmpty()) {
            return std::nullopt;
        }

        bool isOk = false;
        const int parsedValue = digits.toInt(&isOk);
        if (!isOk || parsedValue <= 0) {
            return std::nullopt;
        }
        return parsedValue;
    };

    std::vector<std::vector<int>> hipSequences;
    const auto collectHipSequences = [&](const auto& self, const QJsonValue& value) -> void {
        if (value.isArray()) {
            const QJsonArray array = value.toArray();
            std::vector<int> directHips;
            directHips.reserve(array.size());

            bool hasNestedCollections = false;
            for (const QJsonValue& child : array) {
                if (const auto hip = parseHipIdentifier(child); hip.has_value()) {
                    directHips.push_back(*hip);
                }
                if (child.isArray() || child.isObject()) {
                    hasNestedCollections = true;
                }
            }

            if (directHips.size() >= 2) {
                hipSequences.push_back(std::move(directHips));
            }

            if (hasNestedCollections) {
                for (const QJsonValue& child : array) {
                    self(self, child);
                }
            }
            return;
        }

        if (value.isObject()) {
            const QJsonObject object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                self(self, it.value());
            }
        }
    };

    if (!constellationsValue.isUndefined()) {
        collectHipSequences(collectHipSequences, constellationsValue);
    }
    const QJsonValue asterismsValue = document.object().value("asterisms");
    if (!asterismsValue.isUndefined()) {
        collectHipSequences(collectHipSequences, asterismsValue);
    }

    for (const auto& sequence : hipSequences) {
        for (std::size_t index = 1; index < sequence.size(); ++index) {
            const std::string startId = "hip_" + std::to_string(sequence[index - 1]);
            const std::string endId = "hip_" + std::to_string(sequence[index]);
            const std::string dedupKey = startId + "|" + endId;
            if (!dedupKeys.insert(dedupKey).second) {
                continue;
            }
            lineRefs.emplace_back(startId, endId);
        }
    }

    return lineRefs;
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
        return QColor(214, 224, 250, 220);
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
