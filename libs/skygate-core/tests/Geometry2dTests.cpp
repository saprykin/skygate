#include <QtTest>

#include "skygate/core/math/Geometry2d.hpp"

#include <cstdint>
#include <cmath>
#include <limits>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-9)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class Geometry2dTests final : public QObject {
    Q_OBJECT

private slots:
    void squaredDistance2dUsesCartesianDelta();
    void length2dUsesHypotenuse();
    void rotatedOffsetPoint2dAppliesDegreeRotation();
    void ellipseOffsetPoint2dUsesParametricSamples();
    void ellipseOffsetPoint2dHandlesInvalidSampleCounts();
    void perpendicularOffset2dBuildsNormalOffset();
    void gridHelpersUseFlooredCells();
    void gridHelpersHandleInvalidInputs();
    void areaScaleUsesSquareRootOfAreaRatio();
    void areaScaleFallsBackForInvalidInputs();
    void rectFitIncludesExactMargins();
};

void Geometry2dTests::squaredDistance2dUsesCartesianDelta()
{
    QCOMPARE(skygate::core::squaredDistance2d(1.0, 2.0, 4.0, 6.0), 25.0);
    QCOMPARE(skygate::core::squaredDistance2d(4.0, 6.0, 1.0, 2.0), 25.0);
    QCOMPARE(skygate::core::squaredDistance2d(-2.0, 3.0, -2.0, 3.0), 0.0);
}

void Geometry2dTests::length2dUsesHypotenuse()
{
    QCOMPARE(skygate::core::length2d(3.0, 4.0), 5.0);
    QCOMPARE(skygate::core::length2d(0.0, 0.0), 0.0);
}

void Geometry2dTests::rotatedOffsetPoint2dAppliesDegreeRotation()
{
    const auto quarterTurn = skygate::core::rotatedOffsetPoint2d(
        10.0,
        20.0,
        3.0,
        0.0,
        90.0
    );
    QVERIFY(isNear(quarterTurn.x, 10.0));
    QVERIFY(isNear(quarterTurn.y, 23.0));

    const auto halfTurn = skygate::core::rotatedOffsetPoint2d(
        10.0,
        20.0,
        3.0,
        4.0,
        180.0
    );
    QVERIFY(isNear(halfTurn.x, 7.0));
    QVERIFY(isNear(halfTurn.y, 16.0));
}

void Geometry2dTests::ellipseOffsetPoint2dUsesParametricSamples()
{
    const auto start = skygate::core::ellipseOffsetPoint2d(8.0, 4.0, 0, 4);
    QVERIFY(isNear(start.x, 8.0));
    QVERIFY(isNear(start.y, 0.0));

    const auto quarter = skygate::core::ellipseOffsetPoint2d(8.0, 4.0, 1, 4);
    QVERIFY(isNear(quarter.x, 0.0));
    QVERIFY(isNear(quarter.y, 4.0));
}

void Geometry2dTests::ellipseOffsetPoint2dHandlesInvalidSampleCounts()
{
    const auto zeroSamples = skygate::core::ellipseOffsetPoint2d(8.0, 4.0, 5, 0);
    QCOMPARE(zeroSamples.x, 8.0);
    QCOMPARE(zeroSamples.y, 0.0);

    const auto negativeSamples = skygate::core::ellipseOffsetPoint2d(8.0, 4.0, 5, -1);
    QCOMPARE(negativeSamples.x, 8.0);
    QCOMPARE(negativeSamples.y, 0.0);
}

void Geometry2dTests::perpendicularOffset2dBuildsNormalOffset()
{
    const auto offset = skygate::core::perpendicularOffset2d(
        0.0,
        0.0,
        3.0,
        4.0,
        2.5
    );
    QVERIFY(offset.has_value());
    QCOMPARE(offset->x, -2.0);
    QCOMPARE(offset->y, 1.5);
    QVERIFY(!skygate::core::perpendicularOffset2d(1.0, 1.0, 1.0, 1.0, 2.5).has_value());
}

void Geometry2dTests::gridHelpersUseFlooredCells()
{
    QCOMPARE(skygate::core::gridCellIndex(24.0, 10.0), 2);
    QCOMPARE(skygate::core::gridCellIndex(-1.0, 10.0), -1);
    QCOMPARE(
        skygate::core::packedGridCellKey(2, 3),
        (static_cast<std::uint64_t>(2U) << 32U) | 3U
    );

    const auto center = skygate::core::gridCellCenter(24.0, 36.0, 10.0);
    QCOMPARE(center.x, 25.0);
    QCOMPARE(center.y, 35.0);
}

void Geometry2dTests::gridHelpersHandleInvalidInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    QCOMPARE(skygate::core::gridCellIndex(nan, 10.0), 0);
    QCOMPARE(skygate::core::gridCellIndex(24.0, nan), 0);
    QCOMPARE(skygate::core::gridCellIndex(infinity, 10.0), 0);
    QCOMPARE(skygate::core::gridCellIndex(24.0, 0.0), 0);
    QCOMPARE(skygate::core::gridCellIndex(24.0, -10.0), 0);
    QCOMPARE(
        skygate::core::packedGridCellKey(24.0, 36.0, 10.0),
        skygate::core::packedGridCellKey(2, 3)
    );
}

void Geometry2dTests::areaScaleUsesSquareRootOfAreaRatio()
{
    QCOMPARE(skygate::core::areaScale(2200.0, 1520.0, 1100.0, 760.0), 2.0);
    QCOMPARE(skygate::core::areaScale(0.0, 1520.0, 1100.0, 760.0), 1.0);
}

void Geometry2dTests::areaScaleFallsBackForInvalidInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QCOMPARE(skygate::core::areaScale(nan, 1520.0, 1100.0, 760.0), 1.0);
    QCOMPARE(skygate::core::areaScale(2200.0, -1520.0, 1100.0, 760.0), 1.0);
    QCOMPARE(skygate::core::areaScale(2200.0, 1520.0, 0.0, 760.0), 1.0);
    QCOMPARE(skygate::core::areaScale(2200.0, 1520.0, 1100.0, -760.0), 1.0);
}

void Geometry2dTests::rectFitIncludesExactMargins()
{
    const skygate::core::Rect2d rect {
        .left = 10.0,
        .top = 15.0,
        .right = 90.0,
        .bottom = 85.0
    };

    QVERIFY(skygate::core::fitsWithin(rect, 100.0, 100.0, 10.0));
    QVERIFY(!skygate::core::fitsWithin(rect, 100.0, 100.0, 15.1));
    QVERIFY(skygate::core::fitsWithin(rect, 100.0, 100.0, -5.0));
}

QTEST_APPLESS_MAIN(Geometry2dTests)

#include "Geometry2dTests.moc"
