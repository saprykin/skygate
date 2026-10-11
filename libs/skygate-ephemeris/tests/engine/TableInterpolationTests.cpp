#include "engine/TableInterpolation.hpp"

#include <QtTest/QtTest>

#include <limits>

class TableInterpolationTests final : public QObject {
    Q_OBJECT

private slots:
    void ratioAtIntervalStartIsZero();
    void ratioAtMidpointIsOneHalf();
    void ratioAtIntervalEndIsOne();
    void rejectsEmptyAndReversedIntervals();
    void rejectsUndefinedIntervalWidths();
};

void TableInterpolationTests::ratioAtIntervalStartIsZero()
{
    using skygate::ephemeris::TableInterpolation;

    QCOMPARE(TableInterpolation::ratio(10.0, 10.0, 20.0).value_or(-1.0), 0.0);
}

void TableInterpolationTests::ratioAtMidpointIsOneHalf()
{
    using skygate::ephemeris::TableInterpolation;

    QCOMPARE(TableInterpolation::ratio(15.0, 10.0, 20.0).value_or(-1.0), 0.5);
}

void TableInterpolationTests::ratioAtIntervalEndIsOne()
{
    using skygate::ephemeris::TableInterpolation;

    QCOMPARE(TableInterpolation::ratio(20.0, 10.0, 20.0).value_or(-1.0), 1.0);
}

void TableInterpolationTests::rejectsEmptyAndReversedIntervals()
{
    using skygate::ephemeris::TableInterpolation;

    QVERIFY(!TableInterpolation::ratio(10.0, 10.0, 10.0).has_value());
    QVERIFY(!TableInterpolation::ratio(10.0, 20.0, 10.0).has_value());
    QVERIFY(!TableInterpolation::ratio(10.0, 0.0, -5.0).has_value());
}

void TableInterpolationTests::rejectsUndefinedIntervalWidths()
{
    using skygate::ephemeris::TableInterpolation;

    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QVERIFY(!TableInterpolation::ratio(10.0, nan, 20.0).has_value());
    QVERIFY(!TableInterpolation::ratio(10.0, 0.0, nan).has_value());
    QVERIFY(!TableInterpolation::ratio(10.0, infinity, infinity).has_value());
}

QTEST_MAIN(TableInterpolationTests)

#include "TableInterpolationTests.moc"
