#include "time/AstronomicalEpoch.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/CalendarTime.hpp"
#include "time/CivilDateTime.hpp"
#include "time/EpochCodec.hpp"
#include "time/TimeScale.hpp"
#include "UtcTimeCodec.hpp"
#include "UtcTimePoint.hpp"
#include "math/TimeConstants.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-9)
{
    return std::abs(actual - expected) <= tolerance;
}

[[nodiscard]] skygate::core::AstronomicalEpoch makeEpoch(
    const int year, const int month, const int day, const int hour = 0, const int minute = 0, const int second = 0
)
{
    const auto epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .second = second,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] skygate::core::UtcTimePoint j2000UtcTime()
{
    return skygate::core::UtcTimeCodec::fromEpochMicros(946'728'000'000'000LL);
}

}  // namespace

class TimeCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void calendarTimeMapsJ2000CivilDateToJulianDate();
    void calendarTimeRoundTripsCivilDateAndEpoch();
    void calendarTimeRejectsInvalidCivilDate();
    void civilDateFromUtcPartsValidatesInput();
    void epochCodecMapsJ2000UtcToJulianDate();
    void epochCodecRoundTripsUtcAndEpoch();
    void epochCodecDaysSinceJ2000FromEpoch();
    void epochCodecMapsNtpTimestamps();
    void astronomicalEpochAddsSecondsAndNormalizes();
    void astronomicalEpochComparesEqual();
    void astronomicalTimeReportsJ2000SiderealTime();
};

void TimeCodecTests::calendarTimeMapsJ2000CivilDateToJulianDate()
{
    const skygate::core::AstronomicalEpoch epoch = makeEpoch(2000, 1, 1, 12, 0, 0);

    QVERIFY(isNear(epoch.julianDatePart1 + epoch.julianDatePart2, skygate::core::TimeConstants::kJulianDateJ2000));
    QCOMPARE(static_cast<std::uint8_t>(epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc));
}

void TimeCodecTests::calendarTimeRoundTripsCivilDateAndEpoch()
{
    const skygate::core::AstronomicalEpoch epoch = makeEpoch(2026, 4, 15, 6, 7, 8);
    const auto date = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch);

    QVERIFY(date.has_value());
    QCOMPARE(date->astronomicalYear, 2026);
    QCOMPARE(date->month, 4);
    QCOMPARE(date->day, 15);
    QCOMPARE(date->hour, 6);
    QCOMPARE(date->minute, 7);
    QCOMPARE(date->second, 8);
    QCOMPARE(static_cast<std::uint8_t>(date->timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc));
}

void TimeCodecTests::calendarTimeRejectsInvalidCivilDate()
{
    const skygate::core::CivilDateTime invalid{
        .astronomicalYear = 2024,
        .month = 2,
        .day = 30,
        .timeScale = skygate::core::TimeScale::Utc,
    };

    QVERIFY(!skygate::core::CalendarTime::isValidCivilDateTime(invalid));
    QVERIFY(!skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(invalid).has_value());
}

void TimeCodecTests::civilDateFromUtcPartsValidatesInput()
{
    const auto yearZeroLeapDay = skygate::core::CalendarTime::civilDateFromUtcParts(0, 2, 29);
    QVERIFY(yearZeroLeapDay.has_value());
    QCOMPARE(yearZeroLeapDay->astronomicalYear, 0);
    QCOMPARE(yearZeroLeapDay->month, 2);
    QCOMPARE(yearZeroLeapDay->day, 29);
    QCOMPARE(yearZeroLeapDay->hour, 0);
    QCOMPARE(yearZeroLeapDay->minute, 0);
    QCOMPARE(yearZeroLeapDay->second, 0);
    QCOMPARE(
        static_cast<std::uint8_t>(yearZeroLeapDay->timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc)
    );

    const auto negativeYear = skygate::core::CalendarTime::civilDateFromUtcParts(-44, 3, 15);
    QVERIFY(negativeYear.has_value());
    QCOMPARE(negativeYear->astronomicalYear, -44);
    QCOMPARE(negativeYear->month, 3);
    QCOMPARE(negativeYear->day, 15);

    QVERIFY(skygate::core::CalendarTime::civilDateFromUtcParts(2024, 12, 31).has_value());
    QVERIFY(!skygate::core::CalendarTime::civilDateFromUtcParts(2024, 13, 1).has_value());
    QVERIFY(!skygate::core::CalendarTime::civilDateFromUtcParts(2024, 0, 1).has_value());
    QVERIFY(!skygate::core::CalendarTime::civilDateFromUtcParts(2024, 2, 30).has_value());
    QVERIFY(!skygate::core::CalendarTime::civilDateFromUtcParts(1900, 2, 29).has_value());
    QVERIFY(!skygate::core::CalendarTime::civilDateFromUtcParts(2024, 1, 0).has_value());
}

void TimeCodecTests::epochCodecMapsJ2000UtcToJulianDate()
{
    const skygate::core::UtcTimePoint utcTime = j2000UtcTime();

    QVERIFY(
        isNear(skygate::core::EpochCodec::julianDayFromUtc(utcTime), skygate::core::TimeConstants::kJulianDateJ2000)
    );
    QVERIFY(isNear(skygate::core::EpochCodec::daysSinceJ2000(utcTime), 0.0));
}

void TimeCodecTests::epochCodecRoundTripsUtcAndEpoch()
{
    const skygate::core::UtcTimePoint utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(1'700'000'000'000'000LL);
    const skygate::core::AstronomicalEpoch epoch = skygate::core::EpochCodec::epochFromUtcTime(utcTime);
    const skygate::core::UtcTimePoint roundTrip = skygate::core::EpochCodec::utcTimeFromEpoch(epoch);

    QVERIFY(
        std::abs(
            skygate::core::UtcTimeCodec::toEpochMicros(roundTrip) - skygate::core::UtcTimeCodec::toEpochMicros(utcTime)
        )
        < 1'000
    );
}

void TimeCodecTests::epochCodecDaysSinceJ2000FromEpoch()
{
    const skygate::core::AstronomicalEpoch j2000Epoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    QCOMPARE(skygate::core::EpochCodec::daysSinceJ2000(j2000Epoch), 0.0);

    const skygate::core::AstronomicalEpoch offsetEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 1.25,
        .timeScale = skygate::core::TimeScale::Tt,
    };
    QCOMPARE(skygate::core::EpochCodec::daysSinceJ2000(offsetEpoch), 1.25);
}

void TimeCodecTests::epochCodecMapsNtpTimestamps()
{
    const skygate::core::AstronomicalEpoch ntpEpoch = skygate::core::EpochCodec::epochFromNtpTimestamp(0U);
    QCOMPARE(ntpEpoch.sortKey(), skygate::core::TimeConstants::kJulianDateNtpEpoch);
    QCOMPARE(static_cast<std::uint8_t>(ntpEpoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc));

    // 1900-01-01 00:00:00 plus 36524.5 days is noon on 2000-01-01.
    const skygate::core::AstronomicalEpoch j2000Epoch =
        skygate::core::EpochCodec::epochFromNtpTimestamp(3'155'716'800ULL);
    QCOMPARE(j2000Epoch.julianDatePart1, skygate::core::TimeConstants::kJulianDateJ2000);
    QCOMPARE(j2000Epoch.julianDatePart2, 0.0);
    QCOMPARE(j2000Epoch.sortKey(), skygate::core::TimeConstants::kJulianDateJ2000);
}

void TimeCodecTests::astronomicalEpochAddsSecondsAndNormalizes()
{
    const skygate::core::AstronomicalEpoch epoch = makeEpoch(2000, 1, 1, 23, 59, 30);
    const skygate::core::AstronomicalEpoch shifted = epoch.addSeconds(45.0);
    const auto date = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(shifted);

    QVERIFY(date.has_value());
    QCOMPARE(date->astronomicalYear, 2000);
    QCOMPARE(date->month, 1);
    QCOMPARE(date->day, 2);
    QCOMPARE(date->hour, 0);
    QCOMPARE(date->minute, 0);
    QCOMPARE(date->second, 15);
}

void TimeCodecTests::astronomicalEpochComparesEqual()
{
    const skygate::core::AstronomicalEpoch lhs = makeEpoch(2026, 4, 15);
    const skygate::core::AstronomicalEpoch rhs = makeEpoch(2026, 4, 15);
    const skygate::core::AstronomicalEpoch other = makeEpoch(2026, 4, 16);

    QVERIFY(lhs == rhs);
    QVERIFY(lhs != other);
}

void TimeCodecTests::astronomicalTimeReportsJ2000SiderealTime()
{
    QVERIFY(isNear(skygate::core::AstronomicalTime::meanObliquityDeg(0.0), 23.4393));
    QVERIFY(isNear(skygate::core::AstronomicalTime::greenwichMeanSiderealTimeDeg(j2000UtcTime()), 280.46061837));
}

QTEST_APPLESS_MAIN(TimeCodecTests)

#include "TimeCodecTests.moc"
