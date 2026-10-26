#include "time/EphemerisDateRange.hpp"

#include <QtTest/QtTest>

#include <limits>

namespace {

[[nodiscard]] skygate::core::AstronomicalEpoch epoch(const double julianDate)
{
    return skygate::core::AstronomicalEpoch{
        .julianDatePart1 = julianDate, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Utc
    };
}

}  // namespace

class EphemerisDateRangeTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsOrderedFiniteEndpoints();
    void rejectsReversedOrNonFiniteEndpoints();
    void containsIncludesRangeBoundaries();
    void containsRejectsEpochsOutsideRange();
    void containsRejectsNonFiniteEpochs();
    void exposesIntervalMatchingMembers();
};

void EphemerisDateRangeTests::acceptsOrderedFiniteEndpoints()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(range.isValid());
}

void EphemerisDateRangeTests::rejectsReversedOrNonFiniteEndpoints()
{
    const skygate::ephemeris::EphemerisDateRange reversed{
        .start = epoch(2451546.0),
        .end = epoch(2451545.0),
    };
    QVERIFY(!reversed.isValid());

    const skygate::ephemeris::EphemerisDateRange nonFinite{
        .start = epoch(std::numeric_limits<double>::quiet_NaN()),
        .end = epoch(2451546.0),
    };
    QVERIFY(!nonFinite.isValid());
}

void EphemerisDateRangeTests::containsIncludesRangeBoundaries()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(range.contains(range.start));
    QVERIFY(range.contains(range.end));
    QVERIFY(range.contains(epoch(2451545.5)));
}

void EphemerisDateRangeTests::containsRejectsEpochsOutsideRange()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(!range.contains(epoch(2451544.5)));
    QVERIFY(!range.contains(epoch(2451546.5)));

    const skygate::ephemeris::EphemerisDateRange reversed{
        .start = epoch(2451546.0),
        .end = epoch(2451545.0),
    };
    QVERIFY(!reversed.contains(epoch(2451545.5)));
}

void EphemerisDateRangeTests::containsRejectsNonFiniteEpochs()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(!range.contains(epoch(std::numeric_limits<double>::infinity())));
    QVERIFY(!range.contains(epoch(std::numeric_limits<double>::quiet_NaN())));
}

void EphemerisDateRangeTests::exposesIntervalMatchingMembers()
{
    const skygate::ephemeris::EphemerisDateRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(range.interval().start == range.start);
    QVERIFY(range.interval().end == range.end);
}

QTEST_MAIN(EphemerisDateRangeTests)

#include "EphemerisDateRangeTests.moc"
