#include "math/TimeConstants.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-12)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class TimeConstantsTests final : public QObject {
    Q_OBJECT

private slots:
    void kSecondsPerDay_is86400();
    void kSecondsPerHour_is3600();
    void kSecondsPerMinute_is60();
    void kMinutesPerHour_is60();
    void kHoursPerDay_is24();
    void kMinutesPerDay_is1440();
    void kDaysPerCommonYear_is365();
    void kDaysPerLeapYear_is366();
    void kJulianDaysPerYear_is365_25();
    void kSecondsPerJulianYear_isDaysPerYearTimesSecondsPerDay();
    void kJulianDateUnixEpoch_hasExpectedValue();
    void kJulianDateJ2000_hasExpectedValue();
    void kJulianDateKnownNewMoon_hasExpectedValue();
    void kTtMinusTaiSeconds_hasExpectedValue();
    void kNanosecondsPerSecond_is1e9();
    void kNanosecondsPerMinute_is60e9();
    void kNanosecondsPerHour_is3600e9();
    void kNanosecondsPerDay_is86400e9();
    void kMicrosecondsPerSecond_is1e6();
    void kMicrosecondsPerDay_is86400e6();
};

void TimeConstantsTests::kSecondsPerDay_is86400()
{
    QCOMPARE(skygate::core::TimeConstants::kSecondsPerDay, 86'400.0);
}

void TimeConstantsTests::kSecondsPerHour_is3600()
{
    QCOMPARE(skygate::core::TimeConstants::kSecondsPerHour, 3'600.0);
}

void TimeConstantsTests::kSecondsPerMinute_is60()
{
    QCOMPARE(skygate::core::TimeConstants::kSecondsPerMinute, 60.0);
}

void TimeConstantsTests::kMinutesPerHour_is60()
{
    QCOMPARE(skygate::core::TimeConstants::kMinutesPerHour, 60.0);
}

void TimeConstantsTests::kHoursPerDay_is24()
{
    QCOMPARE(skygate::core::TimeConstants::kHoursPerDay, 24.0);
}

void TimeConstantsTests::kMinutesPerDay_is1440()
{
    QCOMPARE(skygate::core::TimeConstants::kMinutesPerDay, 1440.0);
}

void TimeConstantsTests::kDaysPerCommonYear_is365()
{
    QCOMPARE(skygate::core::TimeConstants::kDaysPerCommonYear, 365.0);
}

void TimeConstantsTests::kDaysPerLeapYear_is366()
{
    QCOMPARE(skygate::core::TimeConstants::kDaysPerLeapYear, 366.0);
}

void TimeConstantsTests::kJulianDaysPerYear_is365_25()
{
    QCOMPARE(skygate::core::TimeConstants::kJulianDaysPerYear, 365.25);
}

void TimeConstantsTests::kSecondsPerJulianYear_isDaysPerYearTimesSecondsPerDay()
{
    QVERIFY(isNear(
        skygate::core::TimeConstants::kSecondsPerJulianYear,
        skygate::core::TimeConstants::kJulianDaysPerYear * skygate::core::TimeConstants::kSecondsPerDay
    ));
}

void TimeConstantsTests::kJulianDateUnixEpoch_hasExpectedValue()
{
    QCOMPARE(skygate::core::TimeConstants::kJulianDateUnixEpoch, 2'440'587.5);
}

void TimeConstantsTests::kJulianDateJ2000_hasExpectedValue()
{
    QCOMPARE(skygate::core::TimeConstants::kJulianDateJ2000, 2'451'545.0);
}

void TimeConstantsTests::kJulianDateKnownNewMoon_hasExpectedValue()
{
    QCOMPARE(skygate::core::TimeConstants::kJulianDateKnownNewMoon, 2'451'550.1);
    // Known new moon of 2000-01-06 should be ~5.1 days after J2000.
    QVERIFY(isNear(
        skygate::core::TimeConstants::kJulianDateKnownNewMoon - skygate::core::TimeConstants::kJulianDateJ2000,
        5.1,
        1e-9
    ));
}

void TimeConstantsTests::kTtMinusTaiSeconds_hasExpectedValue()
{
    QCOMPARE(skygate::core::TimeConstants::kTtMinusTaiSeconds, 32.184);
}

void TimeConstantsTests::kNanosecondsPerSecond_is1e9()
{
    QCOMPARE(skygate::core::TimeConstants::kNanosecondsPerSecond, 1'000'000'000LL);
}

void TimeConstantsTests::kNanosecondsPerMinute_is60e9()
{
    QCOMPARE(skygate::core::TimeConstants::kNanosecondsPerMinute, 60'000'000'000LL);
}

void TimeConstantsTests::kNanosecondsPerHour_is3600e9()
{
    QCOMPARE(skygate::core::TimeConstants::kNanosecondsPerHour, 3'600'000'000'000LL);
}

void TimeConstantsTests::kNanosecondsPerDay_is86400e9()
{
    QCOMPARE(skygate::core::TimeConstants::kNanosecondsPerDay, 86'400'000'000'000LL);
}

void TimeConstantsTests::kMicrosecondsPerSecond_is1e6()
{
    QCOMPARE(skygate::core::TimeConstants::kMicrosecondsPerSecond, 1'000'000.0);
}

void TimeConstantsTests::kMicrosecondsPerDay_is86400e6()
{
    QCOMPARE(skygate::core::TimeConstants::kMicrosecondsPerDay, 86'400'000'000.0);
}

QTEST_APPLESS_MAIN(TimeConstantsTests)

#include "TimeConstantsTests.moc"
