#pragma once

#include "UtcTimePoint.hpp"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QString>

class SkyQtTimeCodec final {
public:
    enum class ParseError {
        None,
        InvalidDateSyntax,
        InvalidYearMagnitude,
        InvalidCalendarDate,
        InvalidTimeSyntax,
    };

    struct ParseResult final {
        int year = 0;  // Signed civil year: 1 BCE is -1.
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        ParseError error = ParseError::None;
    };

    [[nodiscard]] static QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcDateTime);
    [[nodiscard]] static QString formatDateText(const QDate& date);
    [[nodiscard]] static QString formatDateTimeText(const QDateTime& dateTime, bool includeSeconds = true);
    [[nodiscard]] static QRegularExpression dateInputPattern();
    [[nodiscard]] static ParseResult parseDateTime(const QString& dateText, const QString& timeText);
};
