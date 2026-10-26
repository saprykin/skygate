#include "UtcTimeCodec.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/SkyQtTimeCodec.hpp"

#include <QDateTime>
#include <QTimeZone>
#include <QtTest>

#include <limits>

namespace {

[[nodiscard]] skygate::core::AstronomicalEpoch epoch(const double julianDate)
{
    return skygate::core::AstronomicalEpoch{.julianDatePart1 = julianDate, .julianDatePart2 = 0.0};
}

}  // namespace

class SkyQtTimeCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void preservesMillisecondPrecision();
    void floorsPreEpochMicrosecondsToMilliseconds();
    void parsesCivilComponents();
    void classifiesInvalidInput();
    void parsesZonedWallTimeToUtc();
    void classifiesInvalidZonedInput();
    void formatsBceEpochDateText();
    void formatsDateRangeWithoutDisplayName();
    void formatsDateRangeWithDisplayName();
    void formatsNonFiniteEpochDateAsPlaceholder();
    void formatsUtcOffsetText();
};

void SkyQtTimeCodecTests::preservesMillisecondPrecision()
{
    const QDateTime utcDateTime(QDate(2026, 5, 6), QTime(9, 30, 0, 987), QTimeZone::UTC);
    const auto utcTime = SkyQtTimeCodec::toUtcTimePoint(utcDateTime);

    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(utcTime), utcDateTime.toMSecsSinceEpoch() * 1000);
    QCOMPARE(SkyQtTimeCodec::toQDateTimeUtc(utcTime), utcDateTime);
}

void SkyQtTimeCodecTests::floorsPreEpochMicrosecondsToMilliseconds()
{
    // 1.999 ms before the epoch floors to -2 ms rather than truncating towards zero.
    const auto subMillisecond = skygate::core::UtcTimeCodec::fromEpochMicros(-1'999LL);
    QCOMPARE(SkyQtTimeCodec::toQDateTimeUtc(subMillisecond), QDateTime::fromMSecsSinceEpoch(-2, QTimeZone::UTC));

    const QDateTime bceDateTime(QDate(-44, 3, 15), QTime(12, 0, 0, 123), QTimeZone::UTC);
    const auto bceUtcTime = SkyQtTimeCodec::toUtcTimePoint(bceDateTime);
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMillisecondsFloor(bceUtcTime), bceDateTime.toMSecsSinceEpoch());
    QCOMPARE(SkyQtTimeCodec::toQDateTimeUtc(bceUtcTime), bceDateTime);
}

void SkyQtTimeCodecTests::parsesCivilComponents()
{
    const auto ce = SkyQtTimeCodec::parseDateTime(QStringLiteral("2026-05-06"), QStringLiteral("09:30:30"));
    QCOMPARE(ce.error, SkyQtTimeCodec::ParseError::None);
    QCOMPARE(ce.year, 2026);
    QCOMPARE(ce.month, 5);
    QCOMPARE(ce.day, 6);
    QCOMPARE(ce.hour, 9);
    QCOMPARE(ce.minute, 30);
    QCOMPARE(ce.second, 30);

    const auto bce = SkyQtTimeCodec::parseDateTime(QStringLiteral("0044-03-15 bce"), QStringLiteral("12:00:00"));
    QCOMPARE(bce.error, SkyQtTimeCodec::ParseError::None);
    QCOMPARE(bce.year, -44);
    QCOMPARE(bce.month, 3);
    QCOMPARE(bce.day, 15);

    const auto bc = SkyQtTimeCodec::parseDateTime(QStringLiteral("0044-03-15 BC"), QStringLiteral("12:00:00"));
    QCOMPARE(bc.error, SkyQtTimeCodec::ParseError::None);
    QCOMPARE(bc.year, -44);

    const auto padded = SkyQtTimeCodec::parseDateTime(QStringLiteral("  2026-05-06  "), QStringLiteral(" 09:30:30 "));
    QCOMPARE(padded.error, SkyQtTimeCodec::ParseError::None);
    QCOMPARE(padded.year, 2026);
    QCOMPARE(padded.hour, 9);
}

void SkyQtTimeCodecTests::classifiesInvalidInput()
{
    QCOMPARE(
        SkyQtTimeCodec::parseDateTime(QStringLiteral("2026-5-6"), QStringLiteral("09:30:30")).error,
        SkyQtTimeCodec::ParseError::InvalidDateSyntax
    );
    QCOMPARE(
        SkyQtTimeCodec::parseDateTime(QStringLiteral("0000-01-01"), QStringLiteral("00:00:00")).error,
        SkyQtTimeCodec::ParseError::InvalidYearMagnitude
    );
    QCOMPARE(
        SkyQtTimeCodec::parseDateTime(QStringLiteral("12345678901-01-01"), QStringLiteral("00:00:00")).error,
        SkyQtTimeCodec::ParseError::InvalidYearMagnitude
    );
    QCOMPARE(
        SkyQtTimeCodec::parseDateTime(QStringLiteral("2026-02-30"), QStringLiteral("09:30:30")).error,
        SkyQtTimeCodec::ParseError::InvalidCalendarDate
    );
    QCOMPARE(
        SkyQtTimeCodec::parseDateTime(QStringLiteral("2026-05-06"), QStringLiteral("09:30")).error,
        SkyQtTimeCodec::ParseError::InvalidTimeSyntax
    );
    QCOMPARE(
        SkyQtTimeCodec::parseDateTime(QStringLiteral("2026-05-06"), QStringLiteral("09:30:30.500")).error,
        SkyQtTimeCodec::ParseError::InvalidTimeSyntax
    );
}

void SkyQtTimeCodecTests::parsesZonedWallTimeToUtc()
{
    const auto zurich = SkyQtTimeCodec::parseZonedDateTime(
        QStringLiteral("2026-05-06"),
        QStringLiteral("09:30:30"),
        QTimeZone("Europe/Zurich"),
        SkyQtTimeCodec::ErrorScope::LocalDisplay
    );
    QVERIFY(zurich.isValid());
    QVERIFY(zurich.errorText.isEmpty());
    QCOMPARE(*zurich.utcDateTime, QDateTime(QDate(2026, 5, 6), QTime(7, 30, 30), QTimeZone::UTC));

    const auto utc = SkyQtTimeCodec::parseZonedDateTime(
        QStringLiteral(" 0044-03-15 bc "), QStringLiteral(" 12:00:00 "), QTimeZone::UTC, SkyQtTimeCodec::ErrorScope::Utc
    );
    QVERIFY(utc.isValid());
    QCOMPARE(*utc.utcDateTime, QDateTime(QDate(-44, 3, 15), QTime(12, 0, 0), QTimeZone::UTC));
}

void SkyQtTimeCodecTests::classifiesInvalidZonedInput()
{
    const auto dateSyntax = SkyQtTimeCodec::parseZonedDateTime(
        QStringLiteral("2026-5-6"), QStringLiteral("09:30:30"), QTimeZone::UTC, SkyQtTimeCodec::ErrorScope::Utc
    );
    QVERIFY(!dateSyntax.isValid());
    QVERIFY(!dateSyntax.utcDateTime.has_value());
    QCOMPARE(dateSyntax.errorText, QStringLiteral("Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates."));

    const auto calendarDate = SkyQtTimeCodec::parseZonedDateTime(
        QStringLiteral("2026-02-30"),
        QStringLiteral("09:30:30"),
        QTimeZone("Europe/Zurich"),
        SkyQtTimeCodec::ErrorScope::LocalDisplay
    );
    QVERIFY(!calendarDate.isValid());
    QCOMPARE(calendarDate.errorText, QStringLiteral("Enter a valid date."));

    const auto timeSyntax = SkyQtTimeCodec::parseZonedDateTime(
        QStringLiteral("2026-05-06"), QStringLiteral("09:30"), QTimeZone::UTC, SkyQtTimeCodec::ErrorScope::Utc
    );
    QVERIFY(!timeSyntax.isValid());
    QCOMPARE(timeSyntax.errorText, QStringLiteral("Use HH:mm:ss for the UTC time."));
}

void SkyQtTimeCodecTests::formatsBceEpochDateText()
{
    // Julian date 1721060.0 is noon on 1 January of astronomical year 0, which
    // historical dating calls 1 BCE.
    QCOMPARE(SkyQtTimeCodec::formatAstronomicalEpochDate(epoch(1721060.0)), QStringLiteral("0001-01-01 BCE"));
}

void SkyQtTimeCodecTests::formatsDateRangeWithoutDisplayName()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .id = "earth-orientation",
        .displayName = {},
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QCOMPARE(SkyQtTimeCodec::formatDateRangeText(range), QStringLiteral("2000-01-01 to 2000-01-02"));
}

void SkyQtTimeCodecTests::formatsDateRangeWithDisplayName()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .id = "planetary-kernel",
        .displayName = "DE441",
        .start = epoch(1721060.0),
        .end = epoch(2451545.0),
    };

    QCOMPARE(SkyQtTimeCodec::formatDateRangeText(range), QStringLiteral("DE441: 0001-01-01 BCE to 2000-01-01"));
}

void SkyQtTimeCodecTests::formatsNonFiniteEpochDateAsPlaceholder()
{
    QCOMPARE(
        SkyQtTimeCodec::formatAstronomicalEpochDate(epoch(std::numeric_limits<double>::quiet_NaN())),
        QStringLiteral("--")
    );
}

void SkyQtTimeCodecTests::formatsUtcOffsetText()
{
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetText(0), QStringLiteral("+00:00"));
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetText(5 * 3600 + 30 * 60), QStringLiteral("+05:30"));
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetText(-(3 * 3600 + 30 * 60)), QStringLiteral("-03:30"));

    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetLabel(0), QStringLiteral("UTC+00:00"));
    QCOMPARE(SkyQtTimeCodec::formatUtcOffsetLabel(-(3 * 3600 + 30 * 60)), QStringLiteral("UTC-03:30"));
}

QTEST_APPLESS_MAIN(SkyQtTimeCodecTests)

#include "SkyQtTimeCodecTests.moc"
