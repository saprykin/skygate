#pragma once

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <optional>
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

class SkyContextUtcDateTimeTextCodec final {
public:
    struct ParseResult final {
        QDateTime utcDateTime;
        QString errorText;

        [[nodiscard]] bool isValid() const noexcept;
    };

    [[nodiscard]] static ParseResult parse(
        const QString& utcDateText,
        const QString& utcTimeText
    );
    [[nodiscard]] static QString formatDate(const QDateTime& utcTime);
};

class SkyContextSettings final {
public:
    [[nodiscard]] static QString key(const QString& name);
    [[nodiscard]] static QString defaultCatalogCachePath();
};

class SkyContextCatalogCodec final {
public:
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
};

}  // namespace skygate::ui::internal
