#include "SkyContextControllerSupport.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace {

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

}  // namespace

namespace skygate::ui::internal {

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

}  // namespace skygate::ui::internal
