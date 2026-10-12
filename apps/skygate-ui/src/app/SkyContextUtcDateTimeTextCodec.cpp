#include "SkyContextUtcDateTimeTextCodec.hpp"
#include "time/SkyQtTimeCodec.hpp"

#include <QDate>
#include <QTime>
#include <QTimeZone>

namespace skygate::ui::internal {

bool SkyContextUtcDateTimeTextCodec::ParseResult::isValid() const noexcept
{
    return utcDateTime.isValid() && errorText.isEmpty();
}

SkyContextUtcDateTimeTextCodec::ParseResult
SkyContextUtcDateTimeTextCodec::parse(const QString& utcDateText, const QString& utcTimeText)
{
    const QRegularExpressionMatch dateMatch = SkyQtTimeCodec::dateInputPattern().match(utcDateText.trimmed());
    if (!dateMatch.hasMatch()) {
        return {.utcDateTime = {}, .errorText = "Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates."};
    }

    bool isValidYear = false;
    const int yearMagnitude = dateMatch.captured(1).toInt(&isValidYear);
    if (!isValidYear || yearMagnitude == 0) {
        return {.utcDateTime = {}, .errorText = "Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE."};
    }

    bool isValidMonth = false;
    const int month = dateMatch.captured(2).toInt(&isValidMonth);
    bool isValidDay = false;
    const int day = dateMatch.captured(3).toInt(&isValidDay);
    if (!isValidMonth || !isValidDay) {
        return {.utcDateTime = {}, .errorText = "Enter a valid UTC date."};
    }

    const bool isBce = !dateMatch.captured(4).isEmpty();
    const int year = isBce ? -yearMagnitude : yearMagnitude;
    const QDate date(year, month, day);
    if (!date.isValid()) {
        return {.utcDateTime = {}, .errorText = "Enter a valid UTC date."};
    }

    const QTime time = QTime::fromString(utcTimeText.trimmed(), "HH:mm:ss");
    if (!time.isValid()) {
        return {.utcDateTime = {}, .errorText = "Use HH:mm:ss for the UTC time."};
    }

    const QDateTime utcDateTime(date, time, QTimeZone::UTC);
    if (!utcDateTime.isValid()) {
        return {.utcDateTime = {}, .errorText = "Enter a valid UTC date and time."};
    }

    return {.utcDateTime = utcDateTime, .errorText = {}};
}

QString SkyContextUtcDateTimeTextCodec::formatDate(const QDateTime& utcTime)
{
    const QDate date = utcTime.date();
    if (!date.isValid()) {
        // Invalid instants keep the zeroed placeholder rather than empty text.
        return QStringLiteral("0000-00-00");
    }

    return SkyQtTimeCodec::formatDateText(date);
}

}  // namespace skygate::ui::internal
