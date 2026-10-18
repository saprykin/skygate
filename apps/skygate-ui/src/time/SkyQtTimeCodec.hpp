#pragma once

#include "UtcTimePoint.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QString>
#include <QTimeZone>

class SkyQtTimeCodec final {
public:
    enum class ParseError {
        None,
        InvalidDateSyntax,
        InvalidYearMagnitude,
        InvalidCalendarDate,
        InvalidTimeSyntax,
    };

    enum class ErrorScope {
        Utc,
        LocalDisplay,
    };

    struct ParseResult final {
        int year = 0;  // Signed civil year: 1 BCE is -1.
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        ParseError error = ParseError::None;

        [[nodiscard]] QDateTime toQDateTime(const QTimeZone& timeZone) const;
    };

    [[nodiscard]] static QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcDateTime);
    [[nodiscard]] static QString formatDateText(const QDate& date);
    [[nodiscard]] static QString formatDateTimeText(const QDateTime& dateTime, bool includeSeconds = true);
    [[nodiscard]] static QString formatTimeText(const QDateTime& dateTime, bool includeSeconds = true);
    [[nodiscard]] static QString formatAstronomicalEpochDate(const skygate::core::AstronomicalEpoch& epoch);
    [[nodiscard]] static QString formatDateRangeText(const skygate::ephemeris::EphemerisDateRange& range);
    [[nodiscard]] static QString formatUtcOffsetText(int offsetSeconds);   // +hh:mm
    [[nodiscard]] static QString formatUtcOffsetLabel(int offsetSeconds);  // UTC+hh:mm
    [[nodiscard]] static QString timeZoneLabel(const QDateTime& zonedDateTime);
    // ParseError::None yields the text used when parsed components do not form
    // a valid instant.
    [[nodiscard]] static QString errorText(ParseError error, ErrorScope scope);
    [[nodiscard]] static QRegularExpression dateInputPattern();
    [[nodiscard]] static ParseResult parseDateTime(const QString& dateText, const QString& timeText);
};
