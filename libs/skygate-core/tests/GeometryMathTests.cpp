#include <QtTest>

#include "skygate/core/math/GeometryMath.hpp"

#include <cstdint>

class GeometryMathTests final : public QObject {
    Q_OBJECT

private slots:
    void squaredDistance2dUsesCartesianDelta();
    void perpendicularOffset2dBuildsNormalOffset();
    void gridHelpersUseFlooredCells();
    void areaScaleUsesSquareRootOfAreaRatio();
};

void GeometryMathTests::squaredDistance2dUsesCartesianDelta()
{
    QCOMPARE(skygate::core::GeometryMath::squaredDistance2d(1.0, 2.0, 4.0, 6.0), 25.0);
    QCOMPARE(skygate::core::GeometryMath::squaredDistance2d(4.0, 6.0, 1.0, 2.0), 25.0);
    QCOMPARE(skygate::core::GeometryMath::squaredDistance2d(-2.0, 3.0, -2.0, 3.0), 0.0);
}

void GeometryMathTests::perpendicularOffset2dBuildsNormalOffset()
{
    const auto offset = skygate::core::GeometryMath::perpendicularOffset2d(
        0.0,
        0.0,
        3.0,
        4.0,
        2.5
    );
    QVERIFY(offset.has_value());
    QCOMPARE(offset->x, -2.0);
    QCOMPARE(offset->y, 1.5);
    QVERIFY(!skygate::core::GeometryMath::perpendicularOffset2d(1.0, 1.0, 1.0, 1.0, 2.5).has_value());
}

void GeometryMathTests::gridHelpersUseFlooredCells()
{
    QCOMPARE(skygate::core::GeometryMath::gridCellIndex(24.0, 10.0), 2);
    QCOMPARE(skygate::core::GeometryMath::gridCellIndex(-1.0, 10.0), -1);
    QCOMPARE(
        skygate::core::GeometryMath::packedGridCellKey(2, 3),
        (static_cast<std::uint64_t>(2U) << 32U) | 3U
    );

    const auto center = skygate::core::GeometryMath::gridCellCenter(24.0, 36.0, 10.0);
    QCOMPARE(center.x, 25.0);
    QCOMPARE(center.y, 35.0);
}

void GeometryMathTests::areaScaleUsesSquareRootOfAreaRatio()
{
    QCOMPARE(skygate::core::GeometryMath::areaScale(2200.0, 1520.0, 1100.0, 760.0), 2.0);
    QCOMPARE(skygate::core::GeometryMath::areaScale(0.0, 1520.0, 1100.0, 760.0), 1.0);
}

QTEST_APPLESS_MAIN(GeometryMathTests)

#include "GeometryMathTests.moc"
