#include "SkyQtTimeCodec.hpp"

#include "UtcTimeCodec.hpp"

#include <QTimeZone>

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

    const QString timeFormat = includeSeconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm");
    return QString("%1 %2").arg(formatDateText(dateTime.date()), dateTime.toString(timeFormat));
}
