#pragma once

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QString>

#include "skygate/core/IProjection.hpp"
#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ui::internal {
inline constexpr int kTickIntervalMs = 1000;
inline constexpr int kLocationUpdateTimeoutMs = 5000;
inline constexpr int kSettingsVersion = 1;
inline constexpr int kConstellationLineCacheSchemaVersion = 4;
inline constexpr double kDefaultViewportCenterAltitudeDeg = 45.0;
inline constexpr double kDefaultViewportCenterAzimuthDeg = 180.0;
inline constexpr double kViewportFieldOfViewMinDeg = 20.0;
inline constexpr double kViewportFieldOfViewMaxDeg = 150.0;
inline constexpr double kWheelZoomStepScale = 0.90;
inline constexpr double kWheelAngleDeltaStep = 120.0;
inline constexpr double kMagnitudeCutoffMin = -2.0;
inline constexpr double kMagnitudeCutoffMax = 12.0;
inline constexpr double kViewAltitudeMinDeg = -90.0;
inline constexpr double kViewAltitudeMaxDeg = 90.0;
inline constexpr const char* kCatalogCacheFileName = "catalog-cache-v2.txt";
inline constexpr const char* kHygCatalogPrimaryUrl =
    "https://www.astronexus.com/downloads/catalogs/hygdata_v42.csv.gz";
inline constexpr const char* kStellariumConstellationLinesPrimaryUrl =
    "https://raw.githubusercontent.com/Stellarium/stellarium-skycultures/master/western/index.json";
inline constexpr const char* kStellariumConstellationLinesMirrorUrl =
    "https://raw.githubusercontent.com/Stellarium/stellarium-skycultures/main/western/index.json";
inline constexpr const char* kStellariumConstellationLinesCdnUrl =
    "https://cdn.jsdelivr.net/gh/Stellarium/stellarium-skycultures@master/western/index.json";
inline constexpr std::string_view kStarterCatalogRows =
    "sun|Sun|Sun|-26.74\n"
    "moon|Moon|Moon|-12.74\n"
    "venus|Venus|Planet|-4.92\n"
    "jupiter|Jupiter|Planet|-2.94\n"
    "sirius|Sirius|Star|-1.46\n"
    "vega|Vega|Star|0.03\n"
    "betelgeuse|Betelgeuse|Star|0.50\n";
inline constexpr std::string_view kMajorConstellationsCatalogRows =
    "sirius|Sirius|Star|-1.46\n"
    "canopus|Canopus|Star|-0.74\n"
    "arcturus|Arcturus|Star|-0.05\n"
    "vega|Vega|Star|0.03\n"
    "capella|Capella|Star|0.08\n"
    "rigel|Rigel|Star|0.12\n"
    "procyon|Procyon|Star|0.34\n"
    "betelgeuse|Betelgeuse|Star|0.50\n"
    "orion|Orion|Constellation|1.6\n"
    "ursa_major|Ursa Major|Constellation|1.8\n"
    "ursa_minor|Ursa Minor|Constellation|2.1\n"
    "cassiopeia|Cassiopeia|Constellation|2.2\n"
    "scorpius|Scorpius|Constellation|1.7\n"
    "cygnus|Cygnus|Constellation|1.3\n"
    "taurus|Taurus|Constellation|1.7\n"
    "leo|Leo|Constellation|1.4\n"
    "gemini|Gemini|Constellation|1.6\n"
    "andromeda|Andromeda|Constellation|2.1\n";

std::vector<std::pair<std::string, std::string>> defaultConstellationLineRefs();
std::vector<std::pair<std::string, std::vector<std::string>>> defaultConstellationLabelRefs();
std::vector<std::pair<std::string, std::string>> parseStellariumConstellationLineRefs(
    std::string_view constellationLineRows
);
std::vector<std::pair<std::string, std::string>> parseStellariumIndexJsonConstellationLineRefs(
    const QByteArray& jsonPayload
);
std::vector<std::pair<std::string, std::vector<std::string>>> parseStellariumIndexJsonConstellationLabelRefs(
    const QByteArray& jsonPayload
);

QString formatCoordinate(double value);
QString formatElevation(double value);
QString projectionTypeToString(skygate::core::ProjectionType projectionType);
std::optional<skygate::core::ProjectionType> projectionTypeFromString(const QString& value);
QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcTime);
QString settingsKey(const QString& name);
QString defaultCatalogCachePath();
QByteArray serializeCatalogRows(const std::vector<skygate::ephemeris::CelestialBody>& bodies);
QByteArray serializeConstellationLineRows(
    const std::vector<std::pair<std::string, std::string>>& lineRefs
);
std::vector<std::pair<std::string, std::string>> parseConstellationLineRows(std::string_view rows);
QByteArray serializeConstellationLabelRows(
    const std::vector<std::pair<std::string, std::vector<std::string>>>& labelRefs
);
std::vector<std::pair<std::string, std::vector<std::string>>> parseConstellationLabelRows(
    std::string_view rows
);

double pointSizeForMagnitude(double magnitude);
QColor colorForBodyType(skygate::ephemeris::CelestialBodyType type);
QColor constellationLineColor();
std::string_view hipSuffix(std::string_view value);

double normalizeAzimuthDeg(double azimuthDeg);
double clampAltitudeDeg(double altitudeDeg);
double clampFieldOfViewDeg(double fieldOfViewDeg);

skygate::core::ProjectionParams buildProjectionParams(
    double viewportWidth,
    double viewportHeight,
    double centerAltitudeDeg,
    double centerAzimuthDeg,
    double fieldOfViewDeg
);
} // namespace skygate::ui::internal
