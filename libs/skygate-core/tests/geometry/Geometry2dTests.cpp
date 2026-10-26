#include <QtTest>

#include "math/Geometry2d.hpp"

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
    void rotatePoint2dAppliesQuarterTurns();
    void rotatedOffsetPoint2dAppliesDegreeRotation();
    void ellipseOffsetPoint2dUsesParametricSamples();
    void ellipseOffsetPoint2dHandlesInvalidSampleCounts();
    void perpendicularOffset2dBuildsNormalOffset();
    void gridHelpersUseFlooredCells();
    void gridCellRangeCoversRectCells();
    void gridCellRangeFallsBackForInvalidInputs();
    void gridHelpersHandleInvalidInputs();
    void areaScaleUsesSquareRootOfAreaRatio();
    void areaScaleFallsBackForInvalidInputs();
    void pointWithinMarginComparesAgainstShrunkViewport();
    void rectFitIncludesExactMargins();
};

void Geometry2dTests::squaredDistance2dUsesCartesianDelta()
{
    QCOMPARE(skygate::core::Geometry2d::squaredDistance2d(1.0, 2.0, 4.0, 6.0), 25.0);
    QCOMPARE(skygate::core::Geometry2d::squaredDistance2d(4.0, 6.0, 1.0, 2.0), 25.0);
    QCOMPARE(skygate::core::Geometry2d::squaredDistance2d(-2.0, 3.0, -2.0, 3.0), 0.0);
}

void Geometry2dTests::length2dUsesHypotenuse()
{
    QCOMPARE(skygate::core::Geometry2d::length2d(3.0, 4.0), 5.0);
    QCOMPARE(skygate::core::Geometry2d::length2d(0.0, 0.0), 0.0);
}

void Geometry2dTests::rotatePoint2dAppliesQuarterTurns()
{
    const auto unrotated = skygate::core::Geometry2d::rotatePoint2d(3.0, 4.0, 0.0);
    QVERIFY(isNear(unrotated.x, 3.0));
    QVERIFY(isNear(unrotated.y, 4.0));

    const auto counterClockwiseTurn = skygate::core::Geometry2d::rotatePoint2d(3.0, 4.0, 90.0);
    QVERIFY(isNear(counterClockwiseTurn.x, -4.0));
    QVERIFY(isNear(counterClockwiseTurn.y, 3.0));

    const auto clockwiseTurn = skygate::core::Geometry2d::rotatePoint2d(3.0, 4.0, -90.0);
    QVERIFY(isNear(clockwiseTurn.x, 4.0));
    QVERIFY(isNear(clockwiseTurn.y, -3.0));

    const auto halfTurn = skygate::core::Geometry2d::rotatePoint2d(3.0, 4.0, 180.0);
    QVERIFY(isNear(halfTurn.x, -3.0));
    QVERIFY(isNear(halfTurn.y, -4.0));
}

void Geometry2dTests::rotatedOffsetPoint2dAppliesDegreeRotation()
{
    const auto quarterTurn = skygate::core::Geometry2d::rotatedOffsetPoint2d(10.0, 20.0, 3.0, 0.0, 90.0);
    QVERIFY(isNear(quarterTurn.x, 10.0));
    QVERIFY(isNear(quarterTurn.y, 23.0));

    const auto halfTurn = skygate::core::Geometry2d::rotatedOffsetPoint2d(10.0, 20.0, 3.0, 4.0, 180.0);
    QVERIFY(isNear(halfTurn.x, 7.0));
    QVERIFY(isNear(halfTurn.y, 16.0));
}

void Geometry2dTests::ellipseOffsetPoint2dUsesParametricSamples()
{
    const auto start = skygate::core::Geometry2d::ellipseOffsetPoint2d(8.0, 4.0, 0, 4);
    QVERIFY(isNear(start.x, 8.0));
    QVERIFY(isNear(start.y, 0.0));

    const auto quarter = skygate::core::Geometry2d::ellipseOffsetPoint2d(8.0, 4.0, 1, 4);
    QVERIFY(isNear(quarter.x, 0.0));
    QVERIFY(isNear(quarter.y, 4.0));
}

void Geometry2dTests::ellipseOffsetPoint2dHandlesInvalidSampleCounts()
{
    const auto zeroSamples = skygate::core::Geometry2d::ellipseOffsetPoint2d(8.0, 4.0, 5, 0);
    QCOMPARE(zeroSamples.x, 8.0);
    QCOMPARE(zeroSamples.y, 0.0);

    const auto negativeSamples = skygate::core::Geometry2d::ellipseOffsetPoint2d(8.0, 4.0, 5, -1);
    QCOMPARE(negativeSamples.x, 8.0);
    QCOMPARE(negativeSamples.y, 0.0);
}

void Geometry2dTests::perpendicularOffset2dBuildsNormalOffset()
{
    const auto offset = skygate::core::Geometry2d::perpendicularOffset2d(0.0, 0.0, 3.0, 4.0, 2.5);
    QVERIFY(offset.has_value());
    QCOMPARE(offset->x, -2.0);
    QCOMPARE(offset->y, 1.5);
    QVERIFY(!skygate::core::Geometry2d::perpendicularOffset2d(1.0, 1.0, 1.0, 1.0, 2.5).has_value());
}

void Geometry2dTests::gridHelpersUseFlooredCells()
{
    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(24.0, 10.0), 2);
    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(-1.0, 10.0), -1);
    QCOMPARE(skygate::core::Geometry2d::packedGridCellKey(2, 3), (static_cast<std::uint64_t>(2U) << 32U) | 3U);

    const auto center = skygate::core::Geometry2d::gridCellCenter(24.0, 36.0, 10.0);
    QCOMPARE(center.x, 25.0);
    QCOMPARE(center.y, 35.0);
}

void Geometry2dTests::gridCellRangeCoversRectCells()
{
    const skygate::core::Rect2d negativeRect{.left = -25.0, .top = -15.0, .right = -5.0, .bottom = -5.0};
    const auto negativeRange = skygate::core::Geometry2d::gridCellRange(negativeRect, 10.0);
    QCOMPARE(negativeRange.minX, -3);
    QCOMPARE(negativeRange.maxX, -1);
    QCOMPARE(negativeRange.minY, -2);
    QCOMPARE(negativeRange.maxY, -1);

    const skygate::core::Rect2d singleCellRect{.left = 1.0, .top = 2.0, .right = 9.0, .bottom = 9.0};
    const auto singleCellRange = skygate::core::Geometry2d::gridCellRange(singleCellRect, 10.0);
    QCOMPARE(singleCellRange.minX, 0);
    QCOMPARE(singleCellRange.maxX, 0);
    QCOMPARE(singleCellRange.minY, 0);
    QCOMPARE(singleCellRange.maxY, 0);
}

void Geometry2dTests::gridCellRangeFallsBackForInvalidInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    const skygate::core::Rect2d partlyInvalidRect{.left = nan, .top = 0.0, .right = 15.0, .bottom = 5.0};
    const auto partlyInvalidRange = skygate::core::Geometry2d::gridCellRange(partlyInvalidRect, 10.0);
    QCOMPARE(partlyInvalidRange.minX, 0);
    QCOMPARE(partlyInvalidRange.maxX, 1);
    QCOMPARE(partlyInvalidRange.minY, 0);
    QCOMPARE(partlyInvalidRange.maxY, 0);

    const skygate::core::Rect2d rect{.left = 0.0, .top = 0.0, .right = 25.0, .bottom = 25.0};
    const auto zeroCellSizeRange = skygate::core::Geometry2d::gridCellRange(rect, 0.0);
    QCOMPARE(zeroCellSizeRange.minX, 0);
    QCOMPARE(zeroCellSizeRange.maxX, 0);
    QCOMPARE(zeroCellSizeRange.minY, 0);
    QCOMPARE(zeroCellSizeRange.maxY, 0);

    const auto nanCellSizeRange = skygate::core::Geometry2d::gridCellRange(rect, nan);
    QCOMPARE(nanCellSizeRange.minX, 0);
    QCOMPARE(nanCellSizeRange.maxX, 0);
    QCOMPARE(nanCellSizeRange.minY, 0);
    QCOMPARE(nanCellSizeRange.maxY, 0);
}

void Geometry2dTests::gridHelpersHandleInvalidInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(nan, 10.0), 0);
    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(24.0, nan), 0);
    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(infinity, 10.0), 0);
    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(24.0, 0.0), 0);
    QCOMPARE(skygate::core::Geometry2d::gridCellIndex(24.0, -10.0), 0);
    QCOMPARE(
        skygate::core::Geometry2d::packedGridCellKey(24.0, 36.0, 10.0),
        skygate::core::Geometry2d::packedGridCellKey(2, 3)
    );
}

void Geometry2dTests::areaScaleUsesSquareRootOfAreaRatio()
{
    QCOMPARE(skygate::core::Geometry2d::areaScale(2200.0, 1520.0, 1100.0, 760.0), 2.0);
    QCOMPARE(skygate::core::Geometry2d::areaScale(0.0, 1520.0, 1100.0, 760.0), 1.0);
}

void Geometry2dTests::areaScaleFallsBackForInvalidInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QCOMPARE(skygate::core::Geometry2d::areaScale(nan, 1520.0, 1100.0, 760.0), 1.0);
    QCOMPARE(skygate::core::Geometry2d::areaScale(2200.0, -1520.0, 1100.0, 760.0), 1.0);
    QCOMPARE(skygate::core::Geometry2d::areaScale(2200.0, 1520.0, 0.0, 760.0), 1.0);
    QCOMPARE(skygate::core::Geometry2d::areaScale(2200.0, 1520.0, 1100.0, -760.0), 1.0);
}

void Geometry2dTests::pointWithinMarginComparesAgainstShrunkViewport()
{
    QVERIFY(skygate::core::Geometry2d::pointWithinMargin(50.0, 50.0, 100.0, 80.0, 10.0));
    QVERIFY(skygate::core::Geometry2d::pointWithinMargin(10.0, 10.0, 100.0, 80.0, 10.0));
    QVERIFY(skygate::core::Geometry2d::pointWithinMargin(90.0, 70.0, 100.0, 80.0, 10.0));
    QVERIFY(!skygate::core::Geometry2d::pointWithinMargin(9.9, 50.0, 100.0, 80.0, 10.0));
    QVERIFY(!skygate::core::Geometry2d::pointWithinMargin(50.0, 9.9, 100.0, 80.0, 10.0));
    QVERIFY(!skygate::core::Geometry2d::pointWithinMargin(90.1, 50.0, 100.0, 80.0, 10.0));
    QVERIFY(!skygate::core::Geometry2d::pointWithinMargin(50.0, 70.1, 100.0, 80.0, 10.0));
    QVERIFY(skygate::core::Geometry2d::pointWithinMargin(5.0, 5.0, 100.0, 80.0, 0.0));
    QVERIFY(!skygate::core::Geometry2d::pointWithinMargin(-0.1, 5.0, 100.0, 80.0, 0.0));
}

void Geometry2dTests::rectFitIncludesExactMargins()
{
    const skygate::core::Rect2d rect{.left = 10.0, .top = 15.0, .right = 90.0, .bottom = 85.0};

    QVERIFY(skygate::core::Geometry2d::fitsWithin(rect, 100.0, 100.0, 10.0));
    QVERIFY(!skygate::core::Geometry2d::fitsWithin(rect, 100.0, 100.0, 15.1));
    QVERIFY(skygate::core::Geometry2d::fitsWithin(rect, 100.0, 100.0, -5.0));
}

QTEST_APPLESS_MAIN(Geometry2dTests)

#include "Geometry2dTests.moc"
