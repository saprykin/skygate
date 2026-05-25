#include "MoonPhaseCalculator.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"

#include <QtTest/QtTest>

#include <string>

namespace {

[[nodiscard]] skygate::ephemeris::AstronomicalEpoch
epochFromCivil(const int year, const int month, const int day, const int hour, const int minute)
{
    return *skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
}

}  // namespace

class MoonPhaseCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void newMoonHasLowIllumination();
    void firstQuarterIsNear50Percent();
    void fullMoonHasHighIllumination();
    void lastQuarterIsNear50Percent();
    void phaseNamesCoverAllEightBuckets();
    void deterministicForSameEpoch();
    void illuminationClampedBetweenZeroAndOneHundred();
    void waxingCrescentBeforeFirstQuarter();
    void waningGibbousAfterFullMoon();
};

void MoonPhaseCalculatorTests::newMoonHasLowIllumination()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    // 2000-01-06 18:14 UTC — known New Moon
    const auto epoch = epochFromCivil(2000, 1, 6, 18, 14);

    const auto phase = calculator.compute(epoch);

    QVERIFY(phase.illuminationPercent < 1.0);
    QCOMPARE(QString::fromStdString(phase.phaseName), QString("New Moon"));
}

void MoonPhaseCalculatorTests::firstQuarterIsNear50Percent()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    // 2000-01-06 18:14 + ~7d 9h → First quarter
    const auto epoch = epochFromCivil(2000, 1, 14, 3, 14);

    const auto phase = calculator.compute(epoch);

    QCOMPARE(QString::fromStdString(phase.phaseName), QString("First quarter"));
    QVERIFY(phase.illuminationPercent >= 40.0);
    QVERIFY(phase.illuminationPercent <= 60.0);
}

void MoonPhaseCalculatorTests::fullMoonHasHighIllumination()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    // ~14d 18h after New Moon → Full Moon
    const auto epoch = epochFromCivil(2000, 1, 21, 12, 14);

    const auto phase = calculator.compute(epoch);

    QCOMPARE(QString::fromStdString(phase.phaseName), QString("Full Moon"));
    QVERIFY(phase.illuminationPercent > 99.0);
    QVERIFY(phase.illuminationPercent <= 100.0);
}

void MoonPhaseCalculatorTests::lastQuarterIsNear50Percent()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    // ~22d 3h after New Moon → Last quarter
    const auto epoch = epochFromCivil(2000, 1, 28, 21, 14);

    const auto phase = calculator.compute(epoch);

    QCOMPARE(QString::fromStdString(phase.phaseName), QString("Last quarter"));
    QVERIFY(phase.illuminationPercent >= 40.0);
    QVERIFY(phase.illuminationPercent <= 60.0);
}

void MoonPhaseCalculatorTests::phaseNamesCoverAllEightBuckets()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    // Walk through a full synodic month in ~3.7 day steps, starting just
    // after the 2000-01-06 New Moon reference.
    const auto baseEpoch = epochFromCivil(2000, 1, 6, 18, 14);
    const auto synodicMonthDays = 29.530588853;

    // clang-format off
    struct ExpectedPhase {
        double daysOffset;
        QString expectedName;
    };
    const std::vector<ExpectedPhase> expected = {
        {0.5,              QString("New Moon")},
        {synodicMonthDays * 1.0 / 8.0, QString("Waxing crescent")},
        {synodicMonthDays * 2.0 / 8.0, QString("First quarter")},
        {synodicMonthDays * 3.0 / 8.0, QString("Waxing gibbous")},
        {synodicMonthDays * 4.0 / 8.0, QString("Full Moon")},
        {synodicMonthDays * 5.0 / 8.0, QString("Waning gibbous")},
        {synodicMonthDays * 6.0 / 8.0, QString("Last quarter")},
        {synodicMonthDays * 7.0 / 8.0, QString("Waning crescent")},
    };
    // clang-format on

    for (const auto& [daysOffset, expectedName] : expected) {
        const auto epoch = baseEpoch.addMinutes(static_cast<int>(daysOffset * 24.0 * 60.0));
        const auto phase = calculator.compute(epoch);
        QCOMPARE(QString::fromStdString(phase.phaseName), expectedName);
        QVERIFY(phase.illuminationPercent >= 0.0);
        QVERIFY(phase.illuminationPercent <= 100.0);
    }
}

void MoonPhaseCalculatorTests::deterministicForSameEpoch()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    const auto epoch = epochFromCivil(2024, 3, 21, 12, 0);

    const auto first = calculator.compute(epoch);
    const auto second = calculator.compute(epoch);

    QCOMPARE(first.illuminationPercent, second.illuminationPercent);
    QCOMPARE(QString::fromStdString(first.phaseName), QString::fromStdString(second.phaseName));
}

void MoonPhaseCalculatorTests::illuminationClampedBetweenZeroAndOneHundred()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    // Sample many epochs across a wide range to ensure clamping.
    const auto baseEpoch = epochFromCivil(1950, 1, 1, 0, 0);
    for (int day = 0; day < 365 * 5; day += 37) {
        const auto epoch = baseEpoch.addMinutes(day * 24 * 60);
        const auto phase = calculator.compute(epoch);
        QVERIFY2(
            phase.illuminationPercent >= 0.0 && phase.illuminationPercent <= 100.0,
            qPrintable(QString("illumination %1 out of [0,100] at day %2").arg(phase.illuminationPercent).arg(day))
        );
    }
}

void MoonPhaseCalculatorTests::waxingCrescentBeforeFirstQuarter()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    const auto epoch = epochFromCivil(2000, 1, 10, 12, 0);

    const auto phase = calculator.compute(epoch);

    QCOMPARE(QString::fromStdString(phase.phaseName), QString("Waxing crescent"));
    QVERIFY(phase.illuminationPercent > 0.0);
    QVERIFY(phase.illuminationPercent < 50.0);
}

void MoonPhaseCalculatorTests::waningGibbousAfterFullMoon()
{
    const skygate::ephemeris::MoonPhaseCalculator calculator;
    const auto epoch = epochFromCivil(2000, 1, 24, 0, 0);

    const auto phase = calculator.compute(epoch);

    QCOMPARE(QString::fromStdString(phase.phaseName), QString("Waning gibbous"));
    QVERIFY(phase.illuminationPercent > 50.0);
    QVERIFY(phase.illuminationPercent < 100.0);
}

QTEST_APPLESS_MAIN(MoonPhaseCalculatorTests)

#include "MoonPhaseCalculatorTests.moc"
