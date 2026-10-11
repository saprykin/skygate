#include "time/AstronomicalEpochRange.hpp"
#include "time/TimeScale.hpp"

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

class AstronomicalEpochRangeTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsOrderedFiniteEndpoints();
    void rejectsReversedOrNonFiniteEndpoints();
    void containsIncludesRangeBoundaries();
    void containsRejectsEpochsOutsideRange();
    void containsRejectsNonFiniteEpochs();
    void emptyRangeIsEmptyValidAndContainsNothing();
    void containsRangeAcceptsInnerRanges();
    void containsRangeRejectsOutsideOrReversed();
    void operatorEqualsComparesMemberwise();
};

void AstronomicalEpochRangeTests::acceptsOrderedFiniteEndpoints()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(range.isValid());
}

void AstronomicalEpochRangeTests::rejectsReversedOrNonFiniteEndpoints()
{
    const skygate::core::AstronomicalEpochRange reversed{
        .start = epoch(2451546.0),
        .end = epoch(2451545.0),
    };
    QVERIFY(!reversed.isValid());

    const skygate::core::AstronomicalEpochRange nonFinite{
        .start = epoch(std::numeric_limits<double>::quiet_NaN()),
        .end = epoch(2451546.0),
    };
    QVERIFY(!nonFinite.isValid());
}

void AstronomicalEpochRangeTests::containsIncludesRangeBoundaries()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(range.contains(range.start));
    QVERIFY(range.contains(range.end));
    QVERIFY(range.contains(epoch(2451545.5)));
}

void AstronomicalEpochRangeTests::containsRejectsEpochsOutsideRange()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(!range.contains(epoch(2451544.5)));
    QVERIFY(!range.contains(epoch(2451546.5)));

    const skygate::core::AstronomicalEpochRange reversed{
        .start = epoch(2451546.0),
        .end = epoch(2451545.0),
    };
    QVERIFY(!reversed.contains(epoch(2451545.5)));
}

void AstronomicalEpochRangeTests::containsRejectsNonFiniteEpochs()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(!range.contains(epoch(std::numeric_limits<double>::infinity())));
    QVERIFY(!range.contains(epoch(std::numeric_limits<double>::quiet_NaN())));
}

void AstronomicalEpochRangeTests::emptyRangeIsEmptyValidAndContainsNothing()
{
    const skygate::core::AstronomicalEpochRange range;

    QVERIFY(range.isEmpty());
    QVERIFY(range.isValid());
    QVERIFY(!range.contains(epoch(0.0)));

    const skygate::core::AstronomicalEpochRange spanningZero{
        .start = epoch(-1.0),
        .end = epoch(1.0),
    };
    QVERIFY(!spanningZero.contains(range));
}

void AstronomicalEpochRangeTests::containsRangeAcceptsInnerRanges()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };
    const skygate::core::AstronomicalEpochRange inner{
        .start = epoch(2451545.25),
        .end = epoch(2451545.75),
    };
    const skygate::core::AstronomicalEpochRange equalBoundaries{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };

    QVERIFY(range.contains(inner));
    QVERIFY(range.contains(equalBoundaries));
}

void AstronomicalEpochRangeTests::containsRangeRejectsOutsideOrReversed()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };
    const skygate::core::AstronomicalEpochRange pastEnd{
        .start = epoch(2451545.5),
        .end = epoch(2451546.5),
    };
    const skygate::core::AstronomicalEpochRange pastStart{
        .start = epoch(2451544.5),
        .end = epoch(2451545.5),
    };
    const skygate::core::AstronomicalEpochRange reversed{
        .start = epoch(2451546.0),
        .end = epoch(2451545.0),
    };

    QVERIFY(!range.contains(pastEnd));
    QVERIFY(!range.contains(pastStart));
    QVERIFY(!range.contains(reversed));
}

void AstronomicalEpochRangeTests::operatorEqualsComparesMemberwise()
{
    const skygate::core::AstronomicalEpochRange range{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };
    const skygate::core::AstronomicalEpochRange equal{
        .start = epoch(2451545.0),
        .end = epoch(2451546.0),
    };
    const skygate::core::AstronomicalEpochRange differentEnd{
        .start = epoch(2451545.0),
        .end = epoch(2451546.5),
    };

    QVERIFY(range == equal);
    QVERIFY(!(range == differentEnd));
}

QTEST_APPLESS_MAIN(AstronomicalEpochRangeTests)

#include "AstronomicalEpochRangeTests.moc"
