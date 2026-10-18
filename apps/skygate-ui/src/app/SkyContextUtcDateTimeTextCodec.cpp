#include "SkyContextUtcDateTimeTextCodec.hpp"
#include "time/SkyQtTimeCodec.hpp"

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
    if (parsed.error != SkyQtTimeCodec::ParseError::None) {
        return {
            .utcDateTime = {},
            .errorText = SkyQtTimeCodec::errorText(parsed.error, SkyQtTimeCodec::ErrorScope::Utc),
        };
    }

    const QDateTime utcDateTime = parsed.toQDateTime(QTimeZone::UTC);
    if (!utcDateTime.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = SkyQtTimeCodec::errorText(SkyQtTimeCodec::ParseError::None, SkyQtTimeCodec::ErrorScope::Utc),
        };
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
