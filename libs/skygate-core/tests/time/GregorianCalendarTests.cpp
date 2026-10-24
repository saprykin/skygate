#include "time/CivilDateTime.hpp"
#include "time/GregorianCalendar.hpp"
#include "time/TimeScale.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

using skygate::core::CivilDateTime;
using skygate::core::GregorianCalendar;
using skygate::core::TimeScale;

namespace {

constexpr std::array<int, 12> kCommonMonthLengths{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
constexpr std::array<int, 12> kLeapMonthLengths{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

[[nodiscard]] QString civilDateText(const CivilDateTime& dateTime)
{
    return QStringLiteral("%1-%2-%3").arg(dateTime.astronomicalYear).arg(dateTime.month).arg(dateTime.day);
}

[[nodiscard]] bool hasCivilDate(const CivilDateTime& dateTime, const int year, const int month, const int day)
{
    return dateTime.astronomicalYear == year && dateTime.month == month && dateTime.day == day;
}

void advanceOneDay(CivilDateTime& dateTime)
{
    if (dateTime.day < GregorianCalendar::daysInGregorianMonth(dateTime.astronomicalYear, dateTime.month)) {
        ++dateTime.day;
        return;
    }

    dateTime.day = 1;
    if (dateTime.month < 12) {
        ++dateTime.month;
        return;
    }

    dateTime.month = 1;
    ++dateTime.astronomicalYear;
}

}  // namespace

class GregorianCalendarTests final : public QObject {
    Q_OBJECT

private slots:
    void leapYearFollowsGregorianRules();
    void leapYearRecognizesYearZeroAndNegativeYears();
    void daysInMonthFollowsGregorianMonthLengths();
    void daysInMonthRejectsOutOfRangeMonths();
    void daysFromCivilDateMapsUnixEpoch();
    void daysFromCivilDateMapsAstronomicalYearZero();
    void civilDateFromDaysMapsKnownDays();
    void civilDateFromDaysSetsMidnightAndUtc();
    void civilDateFromDaysSequencesMonthEnds();
    void civilDateFromDaysRoundTripsDaysFromCivilDate();
    void decimalYearMapsYearStarts();
    void decimalYearRoundsToNearestDay();
    void decimalYearClampsToLastDayOfYear();
    void decimalYearUsesFloorForNegativeYears();
    void astronomicalYearRejectsHistoricalYearZero();
    void astronomicalYearFromHistoricalYearMapsYears();
    void historicalYearFromAstronomicalYearMapsYears();
    void historicalAndAstronomicalYearsRoundTrip();
};

void GregorianCalendarTests::leapYearFollowsGregorianRules()
{
    constexpr std::array<int, 5> kLeapYears{4, 400, 2000, 2024, 2400};
    constexpr std::array<int, 5> kCommonYears{1, 100, 1900, 2023, 2100};

    for (const int year : kLeapYears) {
        QVERIFY2(GregorianCalendar::isGregorianLeapYear(year), qPrintable(QStringLiteral("year %1").arg(year)));
    }
    for (const int year : kCommonYears) {
        QVERIFY2(!GregorianCalendar::isGregorianLeapYear(year), qPrintable(QStringLiteral("year %1").arg(year)));
    }
}

void GregorianCalendarTests::leapYearRecognizesYearZeroAndNegativeYears()
{
    constexpr std::array<int, 4> kLeapYears{0, -4, -400, -1600};
    constexpr std::array<int, 4> kCommonYears{-1, -100, -1900, -2001};

    for (const int year : kLeapYears) {
        QVERIFY2(GregorianCalendar::isGregorianLeapYear(year), qPrintable(QStringLiteral("year %1").arg(year)));
    }
    for (const int year : kCommonYears) {
        QVERIFY2(!GregorianCalendar::isGregorianLeapYear(year), qPrintable(QStringLiteral("year %1").arg(year)));
    }
}

void GregorianCalendarTests::daysInMonthFollowsGregorianMonthLengths()
{
    for (int month = 1; month <= 12; ++month) {
        const auto index = static_cast<std::size_t>(month - 1);
        QCOMPARE(GregorianCalendar::daysInGregorianMonth(2023, month), kCommonMonthLengths[index]);
        QCOMPARE(GregorianCalendar::daysInGregorianMonth(2024, month), kLeapMonthLengths[index]);
    }

    // The century rule reaches February as well.
    QCOMPARE(GregorianCalendar::daysInGregorianMonth(1900, 2), 28);
    QCOMPARE(GregorianCalendar::daysInGregorianMonth(2000, 2), 29);
    QCOMPARE(GregorianCalendar::daysInGregorianMonth(0, 2), 29);
    QCOMPARE(GregorianCalendar::daysInGregorianMonth(-4, 2), 29);
    QCOMPARE(GregorianCalendar::daysInGregorianMonth(-100, 2), 28);
}

void GregorianCalendarTests::daysInMonthRejectsOutOfRangeMonths()
{
    constexpr std::array<int, 4> kInvalidMonths{-1, 0, 13, 100};

    for (const int month : kInvalidMonths) {
        QCOMPARE(GregorianCalendar::daysInGregorianMonth(2023, month), 0);
        QCOMPARE(GregorianCalendar::daysInGregorianMonth(2024, month), 0);
    }
}

void GregorianCalendarTests::daysFromCivilDateMapsUnixEpoch()
{
    QCOMPARE(GregorianCalendar::daysFromCivilDate(1970, 1, 1), 0LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(1970, 1, 2), 1LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(1969, 12, 31), -1LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(1970, 2, 28), 58LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(1970, 3, 1), 59LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(2000, 1, 1), 10'957LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(9999, 12, 31), 2'932'896LL);
}

void GregorianCalendarTests::daysFromCivilDateMapsAstronomicalYearZero()
{
    // Astronomical year zero is 1 BCE and a leap year.
    QCOMPARE(GregorianCalendar::daysFromCivilDate(0, 1, 1), -719'528LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(0, 2, 29), -719'469LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(-1, 12, 31), -719'529LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(1, 1, 1), -719'162LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(-4, 2, 29), -720'930LL);
    QCOMPARE(GregorianCalendar::daysFromCivilDate(-4713, 11, 24), -2'440'588LL);
}

void GregorianCalendarTests::civilDateFromDaysMapsKnownDays()
{
    const CivilDateTime unixEpoch = GregorianCalendar::civilDateFromDays(0);
    QCOMPARE(unixEpoch.astronomicalYear, 1970);
    QCOMPARE(unixEpoch.month, 1);
    QCOMPARE(unixEpoch.day, 1);

    const CivilDateTime dayBeforeUnixEpoch = GregorianCalendar::civilDateFromDays(-1);
    QCOMPARE(dayBeforeUnixEpoch.astronomicalYear, 1969);
    QCOMPARE(dayBeforeUnixEpoch.month, 12);
    QCOMPARE(dayBeforeUnixEpoch.day, 31);

    const CivilDateTime leapDay = GregorianCalendar::civilDateFromDays(19'782);
    QCOMPARE(leapDay.astronomicalYear, 2024);
    QCOMPARE(leapDay.month, 2);
    QCOMPARE(leapDay.day, 29);

    const CivilDateTime yearZeroStart = GregorianCalendar::civilDateFromDays(-719'528);
    QCOMPARE(yearZeroStart.astronomicalYear, 0);
    QCOMPARE(yearZeroStart.month, 1);
    QCOMPARE(yearZeroStart.day, 1);

    const CivilDateTime lastDayOfYear9999 = GregorianCalendar::civilDateFromDays(2'932'896);
    QCOMPARE(lastDayOfYear9999.astronomicalYear, 9999);
    QCOMPARE(lastDayOfYear9999.month, 12);
    QCOMPARE(lastDayOfYear9999.day, 31);

    // Day -2440588 is the Julian date zero point.
    const CivilDateTime julianDateZero = GregorianCalendar::civilDateFromDays(-2'440'588);
    QCOMPARE(julianDateZero.astronomicalYear, -4713);
    QCOMPARE(julianDateZero.month, 11);
    QCOMPARE(julianDateZero.day, 24);
}

void GregorianCalendarTests::civilDateFromDaysSetsMidnightAndUtc()
{
    const CivilDateTime dateTime = GregorianCalendar::civilDateFromDays(19'782);

    QCOMPARE(dateTime.hour, 0);
    QCOMPARE(dateTime.minute, 0);
    QCOMPARE(dateTime.second, 0);
    QCOMPARE(dateTime.nanosecond, 0U);
    QCOMPARE(static_cast<std::uint8_t>(dateTime.timeScale), static_cast<std::uint8_t>(TimeScale::Utc));
}

void GregorianCalendarTests::civilDateFromDaysSequencesMonthEnds()
{
    const CivilDateTime leapDay = GregorianCalendar::civilDateFromDays(19'782);
    QVERIFY2(hasCivilDate(leapDay, 2024, 2, 29), qPrintable(civilDateText(leapDay)));

    const CivilDateTime dayAfterLeapDay = GregorianCalendar::civilDateFromDays(19'783);
    QVERIFY2(hasCivilDate(dayAfterLeapDay, 2024, 3, 1), qPrintable(civilDateText(dayAfterLeapDay)));

    const CivilDateTime commonFebruaryEnd = GregorianCalendar::civilDateFromDays(19'416);
    QVERIFY2(hasCivilDate(commonFebruaryEnd, 2023, 2, 28), qPrintable(civilDateText(commonFebruaryEnd)));

    const CivilDateTime commonMarchStart = GregorianCalendar::civilDateFromDays(19'417);
    QVERIFY2(hasCivilDate(commonMarchStart, 2023, 3, 1), qPrintable(civilDateText(commonMarchStart)));

    const CivilDateTime yearZeroLeapDay = GregorianCalendar::civilDateFromDays(-719'469);
    QVERIFY2(hasCivilDate(yearZeroLeapDay, 0, 2, 29), qPrintable(civilDateText(yearZeroLeapDay)));

    const CivilDateTime yearZeroMarchStart = GregorianCalendar::civilDateFromDays(-719'468);
    QVERIFY2(hasCivilDate(yearZeroMarchStart, 0, 3, 1), qPrintable(civilDateText(yearZeroMarchStart)));
}

void GregorianCalendarTests::civilDateFromDaysRoundTripsDaysFromCivilDate()
{
    constexpr int kFirstYear = -4000;
    constexpr int kLastYear = 4000;

    const std::int64_t firstDay = GregorianCalendar::daysFromCivilDate(kFirstYear, 1, 1);
    const std::int64_t lastDay = GregorianCalendar::daysFromCivilDate(kLastYear, 12, 31);

    CivilDateTime expected = GregorianCalendar::civilDateFromDays(firstDay);
    QVERIFY2(hasCivilDate(expected, kFirstYear, 1, 1), qPrintable(civilDateText(expected)));

    for (std::int64_t day = firstDay; day <= lastDay; ++day) {
        const CivilDateTime decoded = GregorianCalendar::civilDateFromDays(day);
        if (!hasCivilDate(decoded, expected.astronomicalYear, expected.month, expected.day)) {
            QFAIL(qPrintable(QStringLiteral("day %1 decoded to %2, expected %3-%4-%5")
                                 .arg(static_cast<qlonglong>(day))
                                 .arg(civilDateText(decoded))
                                 .arg(expected.astronomicalYear)
                                 .arg(expected.month)
                                 .arg(expected.day)));
        }

        const std::int64_t encoded =
            GregorianCalendar::daysFromCivilDate(decoded.astronomicalYear, decoded.month, decoded.day);
        if (encoded != day) {
            QFAIL(qPrintable(QStringLiteral("day %1 re-encoded to %2")
                                 .arg(static_cast<qlonglong>(day))
                                 .arg(static_cast<qlonglong>(encoded))));
        }

        advanceOneDay(expected);
    }
}

void GregorianCalendarTests::decimalYearMapsYearStarts()
{
    const CivilDateTime year2000 = GregorianCalendar::civilDateFromDecimalYear(2000.0);
    QVERIFY2(hasCivilDate(year2000, 2000, 1, 1), qPrintable(civilDateText(year2000)));
    QCOMPARE(year2000.hour, 0);
    QCOMPARE(year2000.minute, 0);
    QCOMPARE(year2000.second, 0);
    QCOMPARE(year2000.nanosecond, 0U);
    QCOMPARE(static_cast<std::uint8_t>(year2000.timeScale), static_cast<std::uint8_t>(TimeScale::Utc));

    const CivilDateTime unixYear = GregorianCalendar::civilDateFromDecimalYear(1970.0);
    QVERIFY2(hasCivilDate(unixYear, 1970, 1, 1), qPrintable(civilDateText(unixYear)));

    const CivilDateTime yearOne = GregorianCalendar::civilDateFromDecimalYear(1.0);
    QVERIFY2(hasCivilDate(yearOne, 1, 1, 1), qPrintable(civilDateText(yearOne)));

    const CivilDateTime yearZero = GregorianCalendar::civilDateFromDecimalYear(0.0);
    QVERIFY2(hasCivilDate(yearZero, 0, 1, 1), qPrintable(civilDateText(yearZero)));

    const CivilDateTime firstYearBce = GregorianCalendar::civilDateFromDecimalYear(-1.0);
    QVERIFY2(hasCivilDate(firstYearBce, -1, 1, 1), qPrintable(civilDateText(firstYearBce)));

    const CivilDateTime year2026 = GregorianCalendar::civilDateFromDecimalYear(2026.0);
    QVERIFY2(hasCivilDate(year2026, 2026, 1, 1), qPrintable(civilDateText(year2026)));
}

void GregorianCalendarTests::decimalYearRoundsToNearestDay()
{
    // A quarter of a leap year is 91.5 days, which rounds up to day 92.
    const CivilDateTime leapQuarter = GregorianCalendar::civilDateFromDecimalYear(2024.25);
    QVERIFY2(hasCivilDate(leapQuarter, 2024, 4, 2), qPrintable(civilDateText(leapQuarter)));

    const CivilDateTime leapHalf = GregorianCalendar::civilDateFromDecimalYear(2024.5);
    QVERIFY2(hasCivilDate(leapHalf, 2024, 7, 2), qPrintable(civilDateText(leapHalf)));

    const CivilDateTime leapThreeQuarters = GregorianCalendar::civilDateFromDecimalYear(2024.75);
    QVERIFY2(hasCivilDate(leapThreeQuarters, 2024, 10, 2), qPrintable(civilDateText(leapThreeQuarters)));

    const CivilDateTime commonQuarter = GregorianCalendar::civilDateFromDecimalYear(2023.25);
    QVERIFY2(hasCivilDate(commonQuarter, 2023, 4, 2), qPrintable(civilDateText(commonQuarter)));

    const CivilDateTime commonHalf = GregorianCalendar::civilDateFromDecimalYear(2023.5);
    QVERIFY2(hasCivilDate(commonHalf, 2023, 7, 3), qPrintable(civilDateText(commonHalf)));

    const CivilDateTime commonThreeQuarters = GregorianCalendar::civilDateFromDecimalYear(2023.75);
    QVERIFY2(hasCivilDate(commonThreeQuarters, 2023, 10, 2), qPrintable(civilDateText(commonThreeQuarters)));

    const CivilDateTime eighth = GregorianCalendar::civilDateFromDecimalYear(2026.125);
    QVERIFY2(hasCivilDate(eighth, 2026, 2, 16), qPrintable(civilDateText(eighth)));

    // The century rule shifts the mapping for 1900 and 2000.
    const CivilDateTime nineteenHundredHalf = GregorianCalendar::civilDateFromDecimalYear(1900.5);
    QVERIFY2(hasCivilDate(nineteenHundredHalf, 1900, 7, 3), qPrintable(civilDateText(nineteenHundredHalf)));

    const CivilDateTime twoThousandHalf = GregorianCalendar::civilDateFromDecimalYear(2000.5);
    QVERIFY2(hasCivilDate(twoThousandHalf, 2000, 7, 2), qPrintable(civilDateText(twoThousandHalf)));
}

void GregorianCalendarTests::decimalYearClampsToLastDayOfYear()
{
    const CivilDateTime commonYearEnd = GregorianCalendar::civilDateFromDecimalYear(2023.9999999);
    QVERIFY2(hasCivilDate(commonYearEnd, 2023, 12, 31), qPrintable(civilDateText(commonYearEnd)));

    const CivilDateTime leapYearEnd = GregorianCalendar::civilDateFromDecimalYear(2024.999999);
    QVERIFY2(hasCivilDate(leapYearEnd, 2024, 12, 31), qPrintable(civilDateText(leapYearEnd)));

    const CivilDateTime yearZeroEnd = GregorianCalendar::civilDateFromDecimalYear(0.9999999);
    QVERIFY2(hasCivilDate(yearZeroEnd, 0, 12, 31), qPrintable(civilDateText(yearZeroEnd)));

    const CivilDateTime nineteenHundredEnd = GregorianCalendar::civilDateFromDecimalYear(1900.9999999);
    QVERIFY2(hasCivilDate(nineteenHundredEnd, 1900, 12, 31), qPrintable(civilDateText(nineteenHundredEnd)));
}

void GregorianCalendarTests::decimalYearUsesFloorForNegativeYears()
{
    // -0.25 lies in astronomical year -1, three quarters into that year.
    const CivilDateTime lateFirstYearBce = GregorianCalendar::civilDateFromDecimalYear(-0.25);
    QVERIFY2(hasCivilDate(lateFirstYearBce, -1, 10, 2), qPrintable(civilDateText(lateFirstYearBce)));

    const CivilDateTime closeToYearZero = GregorianCalendar::civilDateFromDecimalYear(-0.0000001);
    QVERIFY2(hasCivilDate(closeToYearZero, -1, 12, 31), qPrintable(civilDateText(closeToYearZero)));

    const CivilDateTime midSecondYearBce = GregorianCalendar::civilDateFromDecimalYear(-1.5);
    QVERIFY2(hasCivilDate(midSecondYearBce, -2, 7, 3), qPrintable(civilDateText(midSecondYearBce)));

    const CivilDateTime secondYearBceStart = GregorianCalendar::civilDateFromDecimalYear(-2001.0);
    QVERIFY2(hasCivilDate(secondYearBceStart, -2001, 1, 1), qPrintable(civilDateText(secondYearBceStart)));

    const CivilDateTime midThirdMillenniumBce = GregorianCalendar::civilDateFromDecimalYear(-2000.5);
    QVERIFY2(hasCivilDate(midThirdMillenniumBce, -2001, 7, 3), qPrintable(civilDateText(midThirdMillenniumBce)));
}

void GregorianCalendarTests::astronomicalYearRejectsHistoricalYearZero()
{
    const std::optional<int> yearZero = GregorianCalendar::astronomicalYearFromHistoricalYear(0);

    QVERIFY(!yearZero.has_value());
}

void GregorianCalendarTests::astronomicalYearFromHistoricalYearMapsYears()
{
    const std::optional<int> firstYearBce = GregorianCalendar::astronomicalYearFromHistoricalYear(-1);
    QVERIFY(firstYearBce.has_value());
    QCOMPARE(*firstYearBce, 0);

    const std::optional<int> fortyFourBce = GregorianCalendar::astronomicalYearFromHistoricalYear(-44);
    QVERIFY(fortyFourBce.has_value());
    QCOMPARE(*fortyFourBce, -43);

    const std::optional<int> julianDateZeroYear = GregorianCalendar::astronomicalYearFromHistoricalYear(-4713);
    QVERIFY(julianDateZeroYear.has_value());
    QCOMPARE(*julianDateZeroYear, -4712);

    const std::optional<int> firstYear = GregorianCalendar::astronomicalYearFromHistoricalYear(1);
    QVERIFY(firstYear.has_value());
    QCOMPARE(*firstYear, 1);

    const std::optional<int> currentYear = GregorianCalendar::astronomicalYearFromHistoricalYear(2026);
    QVERIFY(currentYear.has_value());
    QCOMPARE(*currentYear, 2026);
}

void GregorianCalendarTests::historicalYearFromAstronomicalYearMapsYears()
{
    QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(0), -1);
    QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(-1), -2);
    QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(-43), -44);
    QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(-4712), -4713);
    QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(1), 1);
    QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(2026), 2026);
}

void GregorianCalendarTests::historicalAndAstronomicalYearsRoundTrip()
{
    constexpr int kFirstYear = -5000;
    constexpr int kLastYear = 5000;

    for (int historicalYear = kFirstYear; historicalYear <= kLastYear; ++historicalYear) {
        const std::optional<int> astronomicalYear =
            GregorianCalendar::astronomicalYearFromHistoricalYear(historicalYear);
        if (historicalYear == 0) {
            QVERIFY(!astronomicalYear.has_value());
            continue;
        }

        QVERIFY2(astronomicalYear.has_value(), qPrintable(QStringLiteral("historical year %1").arg(historicalYear)));
        QCOMPARE(GregorianCalendar::historicalYearFromAstronomicalYear(*astronomicalYear), historicalYear);
    }

    for (int astronomicalYear = kFirstYear; astronomicalYear <= kLastYear; ++astronomicalYear) {
        const int historicalYear = GregorianCalendar::historicalYearFromAstronomicalYear(astronomicalYear);
        const std::optional<int> roundTrip = GregorianCalendar::astronomicalYearFromHistoricalYear(historicalYear);

        QVERIFY2(roundTrip.has_value(), qPrintable(QStringLiteral("astronomical year %1").arg(astronomicalYear)));
        QCOMPARE(*roundTrip, astronomicalYear);
    }
}

QTEST_APPLESS_MAIN(GregorianCalendarTests)

#include "GregorianCalendarTests.moc"
