#pragma once

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ui::internal {

enum class SkyContextLocationSource {
    CurrentDevice,
    City,
    Custom,
};

class SkyContextControllerConstants final {
public:
    static constexpr int kTickIntervalMs = 1000;
    static constexpr int kLocationUpdateTimeoutMs = 5000;
    static constexpr int kSettingsVersion = 2;
    static constexpr int kConstellationLineCacheSchemaVersion = 4;
    static constexpr double kWheelZoomStepScale = 0.90;
    static constexpr double kWheelAngleDeltaStep = 120.0;
    static constexpr double kMagnitudeCutoffMin = -2.0;
    static constexpr double kMagnitudeCutoffMax = 12.0;
    static constexpr const char* kCatalogCacheFileName = "catalog-cache-v2.txt";
    static constexpr const char* kHygCatalogPrimaryUrl =
        "https://www.astronexus.com/downloads/catalogs/hygdata_v42.csv.gz";
    static constexpr const char* kStellariumConstellationLinesPrimaryUrl =
        "https://raw.githubusercontent.com/Stellarium/stellarium-skycultures/master/western/index.json";
    static constexpr const char* kStellariumConstellationLinesMirrorUrl =
        "https://raw.githubusercontent.com/Stellarium/stellarium-skycultures/main/western/index.json";
    static constexpr const char* kStellariumConstellationLinesCdnUrl =
        "https://cdn.jsdelivr.net/gh/Stellarium/stellarium-skycultures@master/western/index.json";
    static constexpr std::string_view kStarterCatalogRows =
        "sun|Sun|Sun|-26.74\n"
        "moon|Moon|Moon|-12.74\n"
        "venus|Venus|Planet|-4.92\n"
        "jupiter|Jupiter|Planet|-2.94\n"
        "sirius|Sirius|Star|-1.46\n"
        "vega|Vega|Star|0.03\n"
        "betelgeuse|Betelgeuse|Star|0.50\n";
    static constexpr std::string_view kMajorConstellationsCatalogRows =
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
};

class SkyContextTextFormatter final {
public:
    [[nodiscard]] static QString formatCoordinate(double value);
    [[nodiscard]] static QString formatElevation(double value);
};

class SkyContextProjectionTypeCodec final {
public:
    [[nodiscard]] static QString toString(skygate::core::ProjectionType projectionType);
    [[nodiscard]] static std::optional<skygate::core::ProjectionType> fromString(
        const QString& value
    );
};

class SkyContextLocationSourceCodec final {
public:
    [[nodiscard]] static QString toString(SkyContextLocationSource locationSource);
    [[nodiscard]] static std::optional<SkyContextLocationSource> fromString(
        const QString& value
    );
    [[nodiscard]] static QStringList availableOptions();
    [[nodiscard]] static SkyContextLocationSource defaultSource();
    [[nodiscard]] static bool isAvailable(SkyContextLocationSource locationSource);
};

class SkyContextTimeCodec final {
public:
    [[nodiscard]] static QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcTime);
};

class SkyContextSettings final {
public:
    [[nodiscard]] static QString key(const QString& name);
    [[nodiscard]] static QString defaultCatalogCachePath();
};

class SkyContextCatalogCodec final {
public:
    [[nodiscard]] static QByteArray serializeCatalogRows(
        std::span<const skygate::ephemeris::CelestialBody> bodies
    );

    [[nodiscard]] static QByteArray serializeConstellationLineRows(
        const std::vector<std::pair<std::string, std::string>>& lineRefs
    );
    [[nodiscard]] static std::vector<std::pair<std::string, std::string>>
    parseConstellationLineRows(std::string_view rows);

    [[nodiscard]] static QByteArray serializeConstellationLabelRows(
        const std::vector<std::pair<std::string, std::vector<std::string>>>& labelRefs
    );
    [[nodiscard]] static std::vector<std::pair<std::string, std::vector<std::string>>>
    parseConstellationLabelRows(std::string_view rows);
};

class SkyContextRenderStyle final {
public:
    [[nodiscard]] static double pointSizeForMagnitude(double magnitude);
    [[nodiscard]] static QColor colorForBodyType(skygate::ephemeris::CelestialBodyType type);
    [[nodiscard]] static QColor constellationLineColor();
    [[nodiscard]] static std::string_view hipSuffix(std::string_view value);
};

}  // namespace skygate::ui::internal
