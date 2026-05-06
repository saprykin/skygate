#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/testsupport/DeterministicFuzz.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>

namespace {

[[nodiscard]] bool isNear(
    const double actual,
    const double expected,
    const double tolerance = 1e-8
)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class GeometryPropertyTests final : public QObject {
    Q_OBJECT

private slots:
    void generatedDistancesStaySymmetricAndNonNegative();
    void generatedRotationsPreserveOffsetLength();
    void generatedEllipseSamplesStayOnParametricEllipse();
    void generatedPerpendicularOffsetsStayNormalToSourceSegment();
    void generatedGridCentersContainOriginalCoordinates();
    void generatedRectIntersectionIsSymmetric();
};

void GeometryPropertyTests::generatedDistancesStaySymmetricAndNonNegative()
{
    skygate::testsupport::DeterministicRng rng(0x600d0001U);

    for (int sample = 0; sample < 512; ++sample) {
        const double x1 = rng.realInRange(-10000.0, 10000.0);
        const double y1 = rng.realInRange(-10000.0, 10000.0);
        const double x2 = rng.realInRange(-10000.0, 10000.0);
        const double y2 = rng.realInRange(-10000.0, 10000.0);

        const double forward = skygate::core::squaredDistance2d(x1, y1, x2, y2);
        const double reverse = skygate::core::squaredDistance2d(x2, y2, x1, y1);
        QVERIFY(std::isfinite(forward));
        QVERIFY(forward >= 0.0);
        QVERIFY(isNear(forward, reverse));
        QVERIFY(isNear(skygate::core::length2d(x1 - x2, y1 - y2), std::sqrt(forward), 1e-7));
    }
}

void GeometryPropertyTests::generatedRotationsPreserveOffsetLength()
{
    skygate::testsupport::DeterministicRng rng(0x600d0002U);

    for (int sample = 0; sample < 512; ++sample) {
        const double originX = rng.realInRange(-500.0, 500.0);
        const double originY = rng.realInRange(-500.0, 500.0);
        const double offsetX = rng.realInRange(-250.0, 250.0);
        const double offsetY = rng.realInRange(-250.0, 250.0);
        const double rotationDeg = rng.realInRange(-1440.0, 1440.0);

        const auto rotated = skygate::core::rotatedOffsetPoint2d(
            originX,
            originY,
            offsetX,
            offsetY,
            rotationDeg
        );
        const double originalLength = skygate::core::length2d(offsetX, offsetY);
        const double rotatedLength = skygate::core::length2d(
            rotated.x - originX,
            rotated.y - originY
        );

        QVERIFY(std::isfinite(rotated.x));
        QVERIFY(std::isfinite(rotated.y));
        QVERIFY(isNear(rotatedLength, originalLength, 1e-7));
    }
}

void GeometryPropertyTests::generatedEllipseSamplesStayOnParametricEllipse()
{
    skygate::testsupport::DeterministicRng rng(0x600d0003U);

    for (int sample = 0; sample < 384; ++sample) {
        const double radiusX = rng.realInRange(0.1, 500.0);
        const double radiusY = rng.realInRange(0.1, 500.0);
        const int sampleCount = rng.intInRange(3, 720);
        const int sampleIndex = rng.intInRange(-sampleCount * 2, sampleCount * 2);

        const auto point = skygate::core::ellipseOffsetPoint2d(
            radiusX,
            radiusY,
            sampleIndex,
            sampleCount
        );
        const double ellipseEquation =
            ((point.x * point.x) / (radiusX * radiusX))
            + ((point.y * point.y) / (radiusY * radiusY));

        QVERIFY(std::isfinite(point.x));
        QVERIFY(std::isfinite(point.y));
        QVERIFY(isNear(ellipseEquation, 1.0, 1e-8));
    }
}

void GeometryPropertyTests::generatedPerpendicularOffsetsStayNormalToSourceSegment()
{
    skygate::testsupport::DeterministicRng rng(0x600d0004U);

    for (int sample = 0; sample < 512; ++sample) {
        const double startX = rng.realInRange(-1000.0, 1000.0);
        const double startY = rng.realInRange(-1000.0, 1000.0);
        const double endX = startX + rng.realInRange(-200.0, 200.0);
        const double endY = startY + rng.realInRange(-200.0, 200.0);
        const double halfWidth = rng.realInRange(0.0, 50.0);
        const auto offset = skygate::core::perpendicularOffset2d(
            startX,
            startY,
            endX,
            endY,
            halfWidth
        );

        if (skygate::core::length2d(endX - startX, endY - startY) <= 0.0) {
            QVERIFY(!offset.has_value());
            continue;
        }

        QVERIFY(offset.has_value());
        const double dot = (endX - startX) * offset->x + (endY - startY) * offset->y;
        QVERIFY(isNear(dot, 0.0, 1e-7));
        QVERIFY(isNear(skygate::core::length2d(offset->x, offset->y), halfWidth, 1e-8));
    }
}

void GeometryPropertyTests::generatedGridCentersContainOriginalCoordinates()
{
    skygate::testsupport::DeterministicRng rng(0x600d0005U);

    for (int sample = 0; sample < 512; ++sample) {
        const double x = rng.realInRange(-10000.0, 10000.0);
        const double y = rng.realInRange(-10000.0, 10000.0);
        const double cellSize = rng.realInRange(0.1, 400.0);
        const auto center = skygate::core::gridCellCenter(x, y, cellSize);
        const std::int32_t cellX = skygate::core::gridCellIndex(x, cellSize);
        const std::int32_t cellY = skygate::core::gridCellIndex(y, cellSize);

        QVERIFY(x >= static_cast<double>(cellX) * cellSize);
        QVERIFY(x < static_cast<double>(cellX + 1) * cellSize);
        QVERIFY(y >= static_cast<double>(cellY) * cellSize);
        QVERIFY(y < static_cast<double>(cellY + 1) * cellSize);
        QVERIFY(isNear(center.x, (static_cast<double>(cellX) + 0.5) * cellSize, 1e-8));
        QVERIFY(isNear(center.y, (static_cast<double>(cellY) + 0.5) * cellSize, 1e-8));
        QCOMPARE(
            skygate::core::packedGridCellKey(x, y, cellSize),
            skygate::core::packedGridCellKey(cellX, cellY)
        );
    }
}

void GeometryPropertyTests::generatedRectIntersectionIsSymmetric()
{
    skygate::testsupport::DeterministicRng rng(0x600d0006U);

    for (int sample = 0; sample < 512; ++sample) {
        const double ax1 = rng.realInRange(-500.0, 500.0);
        const double ay1 = rng.realInRange(-500.0, 500.0);
        const double bx1 = rng.realInRange(-500.0, 500.0);
        const double by1 = rng.realInRange(-500.0, 500.0);
        const skygate::core::Rect2d first {
            .left = ax1,
            .top = ay1,
            .right = ax1 + rng.realInRange(0.1, 200.0),
            .bottom = ay1 + rng.realInRange(0.1, 200.0)
        };
        const skygate::core::Rect2d second {
            .left = bx1,
            .top = by1,
            .right = bx1 + rng.realInRange(0.1, 200.0),
            .bottom = by1 + rng.realInRange(0.1, 200.0)
        };

        QCOMPARE(
            skygate::core::intersects(first, second),
            skygate::core::intersects(second, first)
        );
        QVERIFY(skygate::core::intersects(first, first));
        QVERIFY(skygate::core::intersects(second, second));
    }
}

QTEST_APPLESS_MAIN(GeometryPropertyTests)

#include "GeometryPropertyTests.moc"
