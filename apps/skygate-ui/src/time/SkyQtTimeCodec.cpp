#include "SkyQtTimeCodec.hpp"

#include "UtcTimeCodec.hpp"
#include "time/CalendarTime.hpp"

#include <QTime>
#include <QTimeZone>

#include <cstdlib>

namespace {

qint64 floorDiv(const qint64 numerator, const qint64 denominator) noexcept
{
    qint64 quotient = numerator / denominator;
    const qint64 remainder = numerator % denominator;
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        --quotient;
    }
    return quotient;
}

}  // namespace

QDateTime SkyQtTimeCodec::ParseResult::toQDateTime(const QTimeZone& timeZone) const
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second), timeZone);
}

QDateTime SkyQtTimeCodec::toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const qint64 epochMicros = static_cast<qint64>(skygate::core::UtcTimeCodec::toEpochMicros(utcTime));
    return QDateTime::fromMSecsSinceEpoch(floorDiv(epochMicros, 1000), QTimeZone::UTC);
}

skygate::core::UtcTimePoint SkyQtTimeCodec::toUtcTimePoint(const QDateTime& utcDateTime)
{
    return skygate::core::UtcTimeCodec::fromEpochMicros(
        static_cast<std::int64_t>(utcDateTime.toUTC().toMSecsSinceEpoch()) * 1000
    );
}

QString SkyQtTimeCodec::formatDateText(const QDate& date)
{
    if (!date.isValid()) {
        return {};
    }

    const int year = date.year();
    const qint64 displayYear = year < 0 ? -static_cast<qint64>(year) : static_cast<qint64>(year);
    const QString yearText = QString::number(displayYear).rightJustified(4, QLatin1Char('0'));
    const QString dateText = QString("%1-%2-%3")
                                 .arg(yearText)
                                 .arg(date.month(), 2, 10, QLatin1Char('0'))
                                 .arg(date.day(), 2, 10, QLatin1Char('0'));
    return year < 0 ? QString("%1 BCE").arg(dateText) : dateText;
}

QString SkyQtTimeCodec::formatDateTimeText(const QDateTime& dateTime, const bool includeSeconds)
{
    if (!dateTime.isValid()) {
        return {};
    }

    return QString("%1 %2").arg(formatDateText(dateTime.date()), formatTimeText(dateTime, includeSeconds));
}

QString SkyQtTimeCodec::formatTimeText(const QDateTime& dateTime, const bool includeSeconds)
{
    return dateTime.toString(includeSeconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm"));
}

QString SkyQtTimeCodec::formatAstronomicalEpochDate(const skygate::core::AstronomicalEpoch& epoch)
{
    const auto dateTime = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch);
    if (!dateTime.has_value()) {
        return QStringLiteral("--");
    }

    const int historicalYear =
        skygate::core::CalendarTime::historicalYearFromAstronomicalYear(dateTime->astronomicalYear);
    return formatDateText(QDate(historicalYear, dateTime->month, dateTime->day));
}

QString SkyQtTimeCodec::formatDateRangeText(const skygate::ephemeris::EphemerisDateRange& range)
{
    const QString rangeText =
        QString("%1 to %2").arg(formatAstronomicalEpochDate(range.start), formatAstronomicalEpochDate(range.end));
    if (range.displayName.empty()) {
        return rangeText;
    }

    return QString("%1: %2").arg(QString::fromStdString(range.displayName), rangeText);
}

QString SkyQtTimeCodec::formatUtcOffsetText(const int offsetSeconds)
{
    const QChar sign = offsetSeconds < 0 ? QLatin1Char('-') : QLatin1Char('+');
    const int absoluteSeconds = std::abs(offsetSeconds);
    return QString("%1%2:%3")
        .arg(sign)
        .arg(absoluteSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((absoluteSeconds % 3600) / 60, 2, 10, QLatin1Char('0'));
}

QString SkyQtTimeCodec::formatUtcOffsetLabel(const int offsetSeconds)
{
    return QString("UTC%1").arg(formatUtcOffsetText(offsetSeconds));
}

QString SkyQtTimeCodec::timeZoneLabel(const QDateTime& zonedDateTime)
{
    const QTimeZone timeZone = zonedDateTime.timeZone();
    QString label = timeZone.abbreviation(zonedDateTime).trimmed();
    if (label.isEmpty()) {
        label = timeZone.displayName(zonedDateTime, QTimeZone::ShortName).trimmed();
    }

    const QString offsetLabel = formatUtcOffsetLabel(timeZone.offsetFromUtc(zonedDateTime));
    if (label.isEmpty() || label.startsWith(QLatin1String("GMT")) || label.startsWith(QLatin1Char('+'))
        || label.startsWith(QLatin1Char('-'))) {
        return offsetLabel;
    }
    return label;
}

QString SkyQtTimeCodec::errorText(const ParseError error, const ErrorScope scope)
{
    const bool isUtc = scope == ErrorScope::Utc;
    switch (error) {
    case ParseError::None:
        return isUtc ? QStringLiteral("Enter a valid UTC date and time.")
                     : QStringLiteral("Enter a valid date and time.");
    case ParseError::InvalidDateSyntax:
        return QStringLiteral("Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates.");
    case ParseError::InvalidYearMagnitude:
        return QStringLiteral("Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE.");
    case ParseError::InvalidCalendarDate:
        return isUtc ? QStringLiteral("Enter a valid UTC date.") : QStringLiteral("Enter a valid date.");
    case ParseError::InvalidTimeSyntax:
        return isUtc ? QStringLiteral("Use HH:mm:ss for the UTC time.") : QStringLiteral("Use HH:mm:ss for the time.");
    }

    return {};
}

QRegularExpression SkyQtTimeCodec::dateInputPattern()
{
    static const QRegularExpression pattern(
        R"(^(\d{4,})-(\d{2})-(\d{2})(?:\s+(BCE|BC))?$)", QRegularExpression::CaseInsensitiveOption
    );
    return pattern;
}

SkyQtTimeCodec::ParseResult SkyQtTimeCodec::parseDateTime(const QString& dateText, const QString& timeText)
{
    const QRegularExpressionMatch dateMatch = dateInputPattern().match(dateText.trimmed());
    if (!dateMatch.hasMatch()) {
        return {.error = ParseError::InvalidDateSyntax};
    }

    bool isValidYear = false;
    const int yearMagnitude = dateMatch.captured(1).toInt(&isValidYear);
    if (!isValidYear || yearMagnitude == 0) {
        return {.error = ParseError::InvalidYearMagnitude};
    }

    bool isValidMonth = false;
    const int month = dateMatch.captured(2).toInt(&isValidMonth);
    bool isValidDay = false;
    const int day = dateMatch.captured(3).toInt(&isValidDay);
    if (!isValidMonth || !isValidDay) {
        return {.error = ParseError::InvalidCalendarDate};
    }

    const bool isBce = !dateMatch.captured(4).isEmpty();
    const int year = isBce ? -yearMagnitude : yearMagnitude;
    if (!QDate(year, month, day).isValid()) {
        return {.error = ParseError::InvalidCalendarDate};
    }

    const QTime time = QTime::fromString(timeText.trimmed(), "HH:mm:ss");
    if (!time.isValid()) {
        return {.error = ParseError::InvalidTimeSyntax};
    }

    return {
        .year = year,
        .month = month,
        .day = day,
        .hour = time.hour(),
        .minute = time.minute(),
        .second = time.second(),
    };
}
