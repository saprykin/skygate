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
    void kJulianDaysPerYear_is365_25();
    void kSecondsPerJulianYear_isDaysPerYearTimesSecondsPerDay();
    void kJulianDateUnixEpoch_hasExpectedValue();
    void kJulianDateJ2000_hasExpectedValue();
    void kTtMinusTaiSeconds_hasExpectedValue();
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

void TimeConstantsTests::kTtMinusTaiSeconds_hasExpectedValue()
{
    QCOMPARE(skygate::core::TimeConstants::kTtMinusTaiSeconds, 32.184);
}

QTEST_APPLESS_MAIN(TimeConstantsTests)

#include "TimeConstantsTests.moc"
