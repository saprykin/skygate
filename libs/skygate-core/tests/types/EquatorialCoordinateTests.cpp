#include "EquatorialCoordinate.hpp"

#include <QtTest/QtTest>

#include <limits>

class EquatorialCoordinateTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultCoordinateIsValid();
    void boundaryCoordinatesAreValid();
    void rightAscensionUpperBoundIsExclusive();
    void outOfRangeDeclinationIsInvalid();
    void nonFiniteValuesAreInvalid();
    void equalsComparesBothFields();
};

void EquatorialCoordinateTests::defaultCoordinateIsValid()
{
    const skygate::core::EquatorialCoordinate coordinate;
    QVERIFY(coordinate.isValid());
}

void EquatorialCoordinateTests::boundaryCoordinatesAreValid()
{
    skygate::core::EquatorialCoordinate coordinate{
        .rightAscensionHours = skygate::core::EquatorialCoordinate::kRightAscensionMinHours,
        .declinationDeg = skygate::core::EquatorialCoordinate::kDeclinationMaxDeg,
    };
    QVERIFY(coordinate.isValid());

    coordinate.rightAscensionHours = 23.999;
    coordinate.declinationDeg = skygate::core::EquatorialCoordinate::kDeclinationMinDeg;
    QVERIFY(coordinate.isValid());
}

void EquatorialCoordinateTests::rightAscensionUpperBoundIsExclusive()
{
    skygate::core::EquatorialCoordinate coordinate;
    coordinate.rightAscensionHours = skygate::core::EquatorialCoordinate::kRightAscensionMaxHours;
    QVERIFY(!coordinate.isValid());

    coordinate.rightAscensionHours = -0.01;
    QVERIFY(!coordinate.isValid());

    coordinate.rightAscensionHours = 24.01;
    QVERIFY(!coordinate.isValid());
}

void EquatorialCoordinateTests::outOfRangeDeclinationIsInvalid()
{
    skygate::core::EquatorialCoordinate coordinate;
    coordinate.declinationDeg = 90.01;
    QVERIFY(!coordinate.isValid());

    coordinate.declinationDeg = -90.01;
    QVERIFY(!coordinate.isValid());
}

void EquatorialCoordinateTests::nonFiniteValuesAreInvalid()
{
    skygate::core::EquatorialCoordinate coordinate;
    coordinate.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!coordinate.isValid());
    QVERIFY(!coordinate.isFinite());

    coordinate = {};
    coordinate.declinationDeg = std::numeric_limits<double>::infinity();
    QVERIFY(!coordinate.isValid());
}

void EquatorialCoordinateTests::equalsComparesBothFields()
{
    const skygate::core::EquatorialCoordinate coordinate{.rightAscensionHours = 5.5, .declinationDeg = -12.25};
    QVERIFY(coordinate.equals(coordinate));
    QVERIFY(coordinate.equals({.rightAscensionHours = 5.5, .declinationDeg = -12.25}));

    skygate::core::EquatorialCoordinate other = coordinate;
    other.rightAscensionHours += 1e-12;
    QVERIFY(!coordinate.equals(other));

    other = coordinate;
    other.declinationDeg -= 1e-12;
    QVERIFY(!coordinate.equals(other));
}

QTEST_APPLESS_MAIN(EquatorialCoordinateTests)

#include "EquatorialCoordinateTests.moc"
