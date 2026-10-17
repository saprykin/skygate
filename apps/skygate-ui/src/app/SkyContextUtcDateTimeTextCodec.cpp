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
    const SkyQtTimeCodec::ParseResult parsed = SkyQtTimeCodec::parseDateTime(utcDateText, utcTimeText);
    switch (parsed.error) {
    case SkyQtTimeCodec::ParseError::InvalidDateSyntax:
        return {.utcDateTime = {}, .errorText = "Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates."};
    case SkyQtTimeCodec::ParseError::InvalidYearMagnitude:
        return {.utcDateTime = {}, .errorText = "Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE."};
    case SkyQtTimeCodec::ParseError::InvalidCalendarDate:
        return {.utcDateTime = {}, .errorText = "Enter a valid UTC date."};
    case SkyQtTimeCodec::ParseError::InvalidTimeSyntax:
        return {.utcDateTime = {}, .errorText = "Use HH:mm:ss for the UTC time."};
    case SkyQtTimeCodec::ParseError::None:
        break;
    }

    const QDateTime utcDateTime(
        QDate(parsed.year, parsed.month, parsed.day), QTime(parsed.hour, parsed.minute, parsed.second), QTimeZone::UTC
    );
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
