#include "SkyContextControllerSupport.hpp"

#include <QDate>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTime>
#include <QTimeZone>

#include "skygate/ephemeris/ConstellationDataCodec.hpp"

#include <algorithm>

namespace skygate::ui::internal {
namespace {

const QRegularExpression& utcDateInputPattern()
{
    static const QRegularExpression pattern(
        R"(^(\d{4,})-(\d{2})-(\d{2})(?:\s+(BCE|BC))?$)",
        QRegularExpression::CaseInsensitiveOption
    );
    return pattern;
}

QString formatDateText(const QDate& date)
{
    const int year = date.year();
    const qint64 displayYear = year < 0
        ? -static_cast<qint64>(year)
        : static_cast<qint64>(year);
    const QString yearText = QString::number(displayYear).rightJustified(4, '0');
    const QString dateText = QString("%1-%2-%3")
        .arg(yearText)
        .arg(date.month(), 2, 10, QChar('0'))
        .arg(date.day(), 2, 10, QChar('0'));
    return year < 0
        ? QString("%1 BCE").arg(dateText)
        : dateText;
}

}  // namespace

QString SkyContextTextFormatter::formatCoordinate(const double value)
{
    return QString::number(value, 'f', 6);
}

QString SkyContextTextFormatter::formatElevation(const double value)
{
    return QString::number(value, 'f', 1);
}

QString SkyContextProjectionTypeCodec::toString(const skygate::core::ProjectionType projectionType)
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

std::optional<skygate::core::ProjectionType> SkyContextProjectionTypeCodec::fromString(
    const QString& value
)
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

QString SkyContextLocationSourceCodec::toString(const SkyContextLocationSource locationSource)
{
    switch (locationSource) {
    case SkyContextLocationSource::CurrentDevice:
        return "Current Device";
    case SkyContextLocationSource::City:
        return "City";
    case SkyContextLocationSource::Custom:
        return "Custom";
    }

    return "Custom";
}

std::optional<SkyContextLocationSource> SkyContextLocationSourceCodec::fromString(
    const QString& value
)
{
    const QString normalized = value.trimmed().toCaseFolded();
    if (normalized == "current device") {
        return SkyContextLocationSource::CurrentDevice;
    }

    if (normalized == "city") {
        return SkyContextLocationSource::City;
    }

    if (normalized == "custom") {
        return SkyContextLocationSource::Custom;
    }

    return std::nullopt;
}

QStringList SkyContextLocationSourceCodec::availableOptions()
{
    QStringList options;
#if SKYGATE_HAS_POSITIONING
    options.push_back(toString(SkyContextLocationSource::CurrentDevice));
#endif
    options.push_back(toString(SkyContextLocationSource::City));
    options.push_back(toString(SkyContextLocationSource::Custom));
    return options;
}

SkyContextLocationSource SkyContextLocationSourceCodec::defaultSource()
{
#if SKYGATE_HAS_POSITIONING
    return SkyContextLocationSource::CurrentDevice;
#else
    return SkyContextLocationSource::Custom;
#endif
}

bool SkyContextLocationSourceCodec::isAvailable(const SkyContextLocationSource locationSource)
{
#if SKYGATE_HAS_POSITIONING
    (void)locationSource;
    return true;
#else
    return locationSource != SkyContextLocationSource::CurrentDevice;
#endif
}

QDateTime SkyContextTimeCodec::toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(utcTime.time_since_epoch());
    return QDateTime::fromSecsSinceEpoch(seconds.count(), QTimeZone::UTC);
}

skygate::core::UtcTimePoint SkyContextTimeCodec::toUtcTimePoint(const QDateTime& utcTime)
{
    return skygate::core::UtcTimePoint(std::chrono::seconds(utcTime.toSecsSinceEpoch()));
}

bool SkyContextUtcDateTimeTextCodec::ParseResult::isValid() const noexcept
{
    return utcDateTime.isValid() && errorText.isEmpty();
}

SkyContextUtcDateTimeTextCodec::ParseResult SkyContextUtcDateTimeTextCodec::parse(
    const QString& utcDateText,
    const QString& utcTimeText
)
{
    const QRegularExpressionMatch dateMatch = utcDateInputPattern().match(utcDateText.trimmed());
    if (!dateMatch.hasMatch()) {
        return {
            .utcDateTime = {},
            .errorText = "Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates."
        };
    }

    bool isValidYear = false;
    const int yearMagnitude = dateMatch.captured(1).toInt(&isValidYear);
    if (!isValidYear || yearMagnitude == 0) {
        return {
            .utcDateTime = {},
            .errorText = "Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE."
        };
    }

    bool isValidMonth = false;
    const int month = dateMatch.captured(2).toInt(&isValidMonth);
    bool isValidDay = false;
    const int day = dateMatch.captured(3).toInt(&isValidDay);
    if (!isValidMonth || !isValidDay) {
        return {
            .utcDateTime = {},
            .errorText = "Enter a valid UTC date."
        };
    }

    const bool isBce = !dateMatch.captured(4).isEmpty();
    const int year = isBce
        ? -yearMagnitude
        : yearMagnitude;
    const QDate date(year, month, day);
    if (!date.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = "Enter a valid UTC date."
        };
    }

    const QTime time = QTime::fromString(utcTimeText.trimmed(), "HH:mm:ss");
    if (!time.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = "Use HH:mm:ss for the UTC time."
        };
    }

    const QDateTime utcDateTime(date, time, QTimeZone::UTC);
    if (!utcDateTime.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = "Enter a valid UTC date and time."
        };
    }

    return {
        .utcDateTime = utcDateTime,
        .errorText = {}
    };
}

QString SkyContextUtcDateTimeTextCodec::formatDate(const QDateTime& utcTime)
{
    return formatDateText(utcTime.date());
}

QString SkyContextSettings::key(const QString& name)
{
    return QString("skyContext/%1").arg(name);
}

QString SkyContextSettings::defaultCatalogCachePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {};
    }

    return QDir(appDataPath).filePath(
        QString::fromUtf8(SkyContextControllerConstants::kCatalogCacheFileName)
    );
}

QString SkyContextSettings::defaultDeepSkyCatalogCachePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {};
    }

    return QDir(appDataPath).filePath(
        QString::fromUtf8(SkyContextControllerConstants::kDeepSkyCatalogCacheFileName)
    );
}

QByteArray SkyContextCatalogCodec::serializeConstellationLineRows(
    const std::vector<std::pair<std::string, std::string>>& lineRefs
)
{
    return QByteArray::fromStdString(
        skygate::ephemeris::ConstellationDataCodec::serializeLineRows(lineRefs)
    );
}

std::vector<std::pair<std::string, std::string>> SkyContextCatalogCodec::parseConstellationLineRows(
    const std::string_view rows
)
{
    return skygate::ephemeris::ConstellationDataCodec::parseLineRows(rows);
}

QByteArray SkyContextCatalogCodec::serializeConstellationLabelRows(
    const std::vector<std::pair<std::string, std::vector<std::string>>>& labelRefs
)
{
    return QByteArray::fromStdString(
        skygate::ephemeris::ConstellationDataCodec::serializeLabelRows(labelRefs)
    );
}

std::vector<std::pair<std::string, std::vector<std::string>>>
SkyContextCatalogCodec::parseConstellationLabelRows(const std::string_view rows)
{
    return skygate::ephemeris::ConstellationDataCodec::parseLabelRows(rows);
}

double SkyContextRenderStyle::pointSizeForMagnitude(const double magnitude)
{
    const double normalizedBrightness = std::clamp(1.0 - ((magnitude + 1.5) / 8.0), 0.2, 1.0);
    return 1.8 + normalizedBrightness * 5.0;
}

QColor SkyContextRenderStyle::colorForBodyType(
    const skygate::ephemeris::CelestialBodyType type,
    const SkyThemeRenderPalette& renderPalette
)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return renderPalette.bodySun;
    case skygate::ephemeris::CelestialBodyType::Moon:
        return renderPalette.bodyMoon;
    case skygate::ephemeris::CelestialBodyType::Planet:
        return renderPalette.bodyPlanet;
    case skygate::ephemeris::CelestialBodyType::Star:
        return renderPalette.bodyStar;
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return renderPalette.bodyConstellation;
    case skygate::ephemeris::CelestialBodyType::DeepSkyObject:
        return renderPalette.bodyDeepSkyObject;
    }

    return renderPalette.bodyStar;
}

QColor SkyContextRenderStyle::constellationLineColor(
    const SkyThemeRenderPalette& renderPalette
)
{
    return renderPalette.constellationLine;
}

}  // namespace skygate::ui::internal
