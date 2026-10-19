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

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

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
    void epochCodecBuildsJ2000EpochAtRequestedScale();
    void epochCodecRoundTripKeepsOriginalGrouping();
    void epochCodecMapsNtpTimestamps();
    void astronomicalEpochAddsSecondsAndNormalizes();
    void astronomicalEpochComparesEqual();
    void astronomicalTimeReportsJ2000SiderealTime();
    void calendarTimeAndEpochCodecAgreeOnJulianDate();
    void epochFromUtcTimeExactTracksApproximateEpoch();
    void calendarTimeRecognizesUtcLeapSecondLabels();
    void astronomicalEpochAddsDaysAcrossDayBoundary();
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

void TimeCodecTests::epochCodecBuildsJ2000EpochAtRequestedScale()
{
    const skygate::core::AstronomicalEpoch terrestrialTime =
        skygate::core::EpochCodec::j2000Epoch(skygate::core::TimeScale::Tt);
    QCOMPARE(terrestrialTime.sortKey(), skygate::core::TimeConstants::kJulianDateJ2000);
    QCOMPARE(terrestrialTime.julianDatePart2, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(terrestrialTime.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tt)
    );

    const skygate::core::AstronomicalEpoch universalTime =
        skygate::core::EpochCodec::j2000Epoch(skygate::core::TimeScale::Utc);
    QCOMPARE(universalTime.sortKey(), skygate::core::TimeConstants::kJulianDateJ2000);
    QCOMPARE(
        static_cast<std::uint8_t>(universalTime.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc)
    );
}

void TimeCodecTests::epochCodecRoundTripKeepsOriginalGrouping()
{
    const skygate::core::UtcTimePoint utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(1'700'000'000'654'321LL);
    const skygate::core::AstronomicalEpoch epoch = skygate::core::EpochCodec::epochFromUtcTimeExact(utcTime);
    const skygate::core::UtcTimePoint roundTrip = skygate::core::EpochCodec::utcTimeFromEpoch(epoch);

    // Julian-date round trips quantize at the Julian-date magnitude, so the
    // original grouping resolves this instant to 654304 microseconds.
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(roundTrip), 1'700'000'000'654'304LL);
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

void TimeCodecTests::calendarTimeAndEpochCodecAgreeOnJulianDate()
{
    const skygate::core::UtcTimePoint utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(1'700'000'000'000'000LL);
    const auto calendarEpoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2023,
            .month = 11,
            .day = 14,
            .hour = 22,
            .minute = 13,
            .second = 20,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    QVERIFY(calendarEpoch.has_value());

    const double utcJulianDay = skygate::core::EpochCodec::julianDayFromUtc(utcTime);
    QVERIFY(isNear(calendarEpoch->sortKey(), utcJulianDay, 1e-9));
    QVERIFY(isNear(skygate::core::EpochCodec::julianDayFromEpoch(*calendarEpoch), utcJulianDay, 1e-9));

    const double daysSinceUnix =
        skygate::core::EpochCodec::daysSinceUnixEpoch(calendarEpoch->julianDatePart1, calendarEpoch->julianDatePart2);
    QVERIFY(isNear(skygate::core::EpochCodec::julianDateFromDaysSinceUnixEpoch(daysSinceUnix), utcJulianDay, 1e-9));
}

void TimeCodecTests::epochFromUtcTimeExactTracksApproximateEpoch()
{
    constexpr std::array<std::int64_t, 4> kInstants{
        0LL,
        946'728'000'123'456LL,
        1'700'000'000'654'321LL,
        -1'500'000LL,
    };

    for (const std::int64_t instant : kInstants) {
        const skygate::core::UtcTimePoint utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(instant);
        const skygate::core::AstronomicalEpoch exact = skygate::core::EpochCodec::epochFromUtcTimeExact(utcTime);
        const skygate::core::AstronomicalEpoch approximate = skygate::core::EpochCodec::epochFromUtcTime(utcTime);

        QVERIFY(isNear(exact.julianDatePart1, approximate.julianDatePart1, 0.0));
        QCOMPARE(static_cast<std::uint8_t>(exact.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc));

        const double part2DeltaMicroseconds = std::abs(exact.julianDatePart2 - approximate.julianDatePart2)
                                              * skygate::core::TimeConstants::kMicrosecondsPerDay;
        QVERIFY(part2DeltaMicroseconds < 50.0);
    }

    const auto expected = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2023,
            .month = 11,
            .day = 14,
            .hour = 22,
            .minute = 13,
            .second = 20,
            .nanosecond = 654'321'000U,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    QVERIFY(expected.has_value());
    QVERIFY(
        skygate::core::EpochCodec::epochFromUtcTimeExact(
            skygate::core::UtcTimeCodec::fromEpochMicros(1'700'000'000'654'321LL)
        )
        == *expected
    );
}

void TimeCodecTests::calendarTimeRecognizesUtcLeapSecondLabels()
{
    const skygate::core::CivilDateTime leapSecond{
        .astronomicalYear = 2024,
        .month = 6,
        .day = 30,
        .hour = 23,
        .minute = 59,
        .second = 60,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    QVERIFY(skygate::core::CalendarTime::isValidCivilDateTime(leapSecond));
    QVERIFY(skygate::core::CalendarTime::isUtcLeapSecondLabel(leapSecond));

    const std::optional<skygate::core::CivilDateTime> predecessor =
        skygate::core::CalendarTime::leapSecondPredecessor(leapSecond);
    QVERIFY(predecessor.has_value());
    QCOMPARE(predecessor->astronomicalYear, 2024);
    QCOMPARE(predecessor->month, 6);
    QCOMPARE(predecessor->day, 30);
    QCOMPARE(predecessor->hour, 23);
    QCOMPARE(predecessor->minute, 59);
    QCOMPARE(predecessor->second, 59);
    QVERIFY(skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*predecessor).has_value());

    const skygate::core::CivilDateTime lastRegularSecond{
        .astronomicalYear = 2024,
        .month = 6,
        .day = 30,
        .hour = 23,
        .minute = 59,
        .second = 59,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    QVERIFY(!skygate::core::CalendarTime::isUtcLeapSecondLabel(lastRegularSecond));
    QVERIFY(!skygate::core::CalendarTime::leapSecondPredecessor(lastRegularSecond).has_value());

    const skygate::core::CivilDateTime nonUtcLabel{
        .astronomicalYear = 2024,
        .month = 6,
        .day = 30,
        .hour = 23,
        .minute = 59,
        .second = 60,
        .timeScale = skygate::core::TimeScale::Tt,
    };
    QVERIFY(!skygate::core::CalendarTime::isValidCivilDateTime(nonUtcLabel));
    QVERIFY(!skygate::core::CalendarTime::isUtcLeapSecondLabel(nonUtcLabel));
    QVERIFY(!skygate::core::CalendarTime::leapSecondPredecessor(nonUtcLabel).has_value());
}

void TimeCodecTests::astronomicalEpochAddsDaysAcrossDayBoundary()
{
    const skygate::core::AstronomicalEpoch epoch = makeEpoch(2000, 1, 1, 23, 59, 30);

    const skygate::core::AstronomicalEpoch nextDay = epoch.addDays(1.0);
    QVERIFY(isNear(nextDay.julianDatePart1, epoch.julianDatePart1 + 1.0, 0.0));
    QVERIFY(isNear(nextDay.julianDatePart2, epoch.julianDatePart2, 1e-12));
    const auto nextDate = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(nextDay);
    QVERIFY(nextDate.has_value());
    QCOMPARE(nextDate->astronomicalYear, 2000);
    QCOMPARE(nextDate->month, 1);
    QCOMPARE(nextDate->day, 2);
    QCOMPARE(nextDate->hour, 23);
    QCOMPARE(nextDate->minute, 59);
    QCOMPARE(nextDate->second, 30);

    const auto previousDate = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch.addDays(-1.0));
    QVERIFY(previousDate.has_value());
    QCOMPARE(previousDate->astronomicalYear, 1999);
    QCOMPARE(previousDate->month, 12);
    QCOMPARE(previousDate->day, 31);
    QCOMPARE(previousDate->hour, 23);
    QCOMPARE(previousDate->minute, 59);
    QCOMPARE(previousDate->second, 30);

    QVERIFY(epoch.addDays(0.0) == epoch);
}

QTEST_APPLESS_MAIN(TimeCodecTests)

#include "TimeCodecTests.moc"
