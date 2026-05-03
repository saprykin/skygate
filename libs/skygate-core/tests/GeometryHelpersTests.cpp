#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/core/math/LinePattern.hpp"
#include "skygate/core/math/ProjectedPolylineBuilder.hpp"
#include "skygate/core/math/SpatialIndex2d.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-9)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class GeometryHelpersTests final : public QObject {
    Q_OBJECT

private slots:
    void rectIntersectionAndViewportFit();
    void rectOccupancyGridDetectsCollisions();
    void rectOccupancyGridHandlesEdgesAndNegativeCoordinates();
    void circleHitIndexReturnsNearestPayload();
    void circleHitIndexFiltersTargetsAndSupportsRebuild();
    void projectedPolylineSplitsGapsAndDropsLongJumps();
    void projectedPolylineHandlesBoundaryAndDegenerateInputs();
    void dashedLineBuilderCreatesDashSegments();
    void dashedLineBuilderHandlesDegeneratePatternsAndDiagonals();
    void coordinateFiniteQueriesDoNotChangeValidity();
};

void GeometryHelpersTests::rectIntersectionAndViewportFit()
{
    const skygate::core::Rect2d rect {
        .left = 10.0,
        .top = 20.0,
        .right = 40.0,
        .bottom = 60.0
    };

    QVERIFY(skygate::core::intersects(rect, skygate::core::Rect2d {
        .left = 35.0,
        .top = 50.0,
        .right = 80.0,
        .bottom = 90.0
    }));
    QVERIFY(!skygate::core::intersects(rect, skygate::core::Rect2d {
        .left = 40.0,
        .top = 50.0,
        .right = 80.0,
        .bottom = 90.0
    }));

    QVERIFY(skygate::core::fitsWithin(rect, 100.0, 100.0, 5.0));
    QVERIFY(!skygate::core::fitsWithin(rect, 100.0, 100.0, 25.0));
}

void GeometryHelpersTests::rectOccupancyGridDetectsCollisions()
{
    skygate::core::RectOccupancyGrid grid(16.0);
    const skygate::core::Rect2d occupied {
        .left = 10.0,
        .top = 10.0,
        .right = 30.0,
        .bottom = 30.0
    };

    QVERIFY(!grid.collides(occupied));
    grid.add(occupied);
    QVERIFY(grid.collides(skygate::core::Rect2d {
        .left = 29.0,
        .top = 12.0,
        .right = 50.0,
        .bottom = 20.0
    }));
    QVERIFY(!grid.collides(skygate::core::Rect2d {
        .left = 40.0,
        .top = 40.0,
        .right = 60.0,
        .bottom = 60.0
    }));

    grid.clear();
    QVERIFY(!grid.collides(occupied));
}

void GeometryHelpersTests::rectOccupancyGridHandlesEdgesAndNegativeCoordinates()
{
    skygate::core::RectOccupancyGrid grid(10.0);
    grid.add(skygate::core::Rect2d {
        .left = -20.0,
        .top = -20.0,
        .right = -10.0,
        .bottom = -10.0
    });

    QVERIFY(grid.collides(skygate::core::Rect2d {
        .left = -15.0,
        .top = -15.0,
        .right = -5.0,
        .bottom = -5.0
    }));
    QVERIFY(!grid.collides(skygate::core::Rect2d {
        .left = -10.0,
        .top = -15.0,
        .right = 0.0,
        .bottom = -5.0
    }));

    grid.add(skygate::core::Rect2d {
        .left = 0.0,
        .top = 0.0,
        .right = 10.0,
        .bottom = 10.0
    });
    grid.add(skygate::core::Rect2d {
        .left = 20.0,
        .top = 20.0,
        .right = 30.0,
        .bottom = 30.0
    });
    grid.clear();

    QVERIFY(!grid.collides(skygate::core::Rect2d {
        .left = 5.0,
        .top = 5.0,
        .right = 8.0,
        .bottom = 8.0
    }));
}

void GeometryHelpersTests::circleHitIndexReturnsNearestPayload()
{
    skygate::core::CircleHitIndex index(10.0);
    QVERIFY(!index.nearestPayloadAt(0.0, 0.0).has_value());

    const std::array<skygate::core::CircleHitTarget, 3> targets {{
        {.x = 50.0, .y = 50.0, .radius = 5.0, .payloadId = 1U},
        {.x = 55.0, .y = 50.0, .radius = 10.0, .payloadId = 2U},
        {.x = 100.0, .y = 100.0, .radius = 20.0, .payloadId = 3U},
    }};
    index.rebuild(targets);

    QCOMPARE(index.nearestPayloadAt(55.0, 50.0).value_or(0U), 2U);
    QCOMPARE(index.nearestPayloadAt(115.0, 100.0).value_or(0U), 3U);
    QVERIFY(!index.nearestPayloadAt(150.0, 150.0).has_value());
}

void GeometryHelpersTests::circleHitIndexFiltersTargetsAndSupportsRebuild()
{
    skygate::core::CircleHitIndex index(10.0);
    const std::array<skygate::core::CircleHitTarget, 5> targets {{
        {.x = 0.0, .y = 0.0, .radius = 0.0, .payloadId = 1U},
        {.x = 0.0, .y = 0.0, .radius = -1.0, .payloadId = 2U},
        {
            .x = std::numeric_limits<double>::quiet_NaN(),
            .y = 0.0,
            .radius = 5.0,
            .payloadId = 3U
        },
        {.x = -10.0, .y = -10.0, .radius = 5.0, .payloadId = 4U},
        {.x = -8.0, .y = -10.0, .radius = 5.0, .payloadId = 5U},
    }};
    index.rebuild(targets);

    QCOMPARE(index.nearestPayloadAt(-5.0, -10.0).value_or(0U), 5U);
    QCOMPARE(index.nearestPayloadAt(-15.0, -10.0).value_or(0U), 4U);
    QVERIFY(!index.nearestPayloadAt(0.0, 0.0).has_value());

    const std::array<skygate::core::CircleHitTarget, 1> rebuiltTargets {{
        {.x = 50.0, .y = 50.0, .radius = 10.0, .payloadId = 9U},
    }};
    index.rebuild(rebuiltTargets);
    QVERIFY(!index.nearestPayloadAt(-10.0, -10.0).has_value());
    QCOMPARE(index.nearestPayloadAt(50.0, 50.0).value_or(0U), 9U);

    index.clear();
    QVERIFY(!index.nearestPayloadAt(50.0, 50.0).has_value());
}

void GeometryHelpersTests::projectedPolylineSplitsGapsAndDropsLongJumps()
{
    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ProjectionParams {
            .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
            .fovDeg = 100.0,
            .rollDeg = 0.0,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0
        }
    );
    QVERIFY(projection.has_value());

    const std::vector<skygate::core::HorizontalCoordinate> coordinates {
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        {.altitudeDeg = 46.0, .azimuthDeg = 181.0},
        {.altitudeDeg = std::numeric_limits<double>::quiet_NaN(), .azimuthDeg = 181.0},
        {.altitudeDeg = 47.0, .azimuthDeg = 182.0},
        {.altitudeDeg = 48.0, .azimuthDeg = 183.0},
    };

    const skygate::core::ProjectedPolylineBuilder builder;
    const auto segments = builder.build(*projection, coordinates, 1'000'000.0);
    QCOMPARE(segments.size(), 2U);

    const auto filteredSegments = builder.build(*projection, coordinates, 0.001);
    QVERIFY(filteredSegments.empty());
}

void GeometryHelpersTests::projectedPolylineHandlesBoundaryAndDegenerateInputs()
{
    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ProjectionParams {
            .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
            .fovDeg = 100.0,
            .rollDeg = 0.0,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0
        }
    );
    QVERIFY(projection.has_value());

    const skygate::core::ProjectedPolylineBuilder builder;
    const std::vector<skygate::core::HorizontalCoordinate> oneCoordinate {
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
    };
    QVERIFY(builder.build(*projection, oneCoordinate, 1'000'000.0).empty());

    const std::vector<skygate::core::HorizontalCoordinate> twoCoordinates {
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        {.altitudeDeg = 46.0, .azimuthDeg = 181.0},
    };
    const auto firstPoint = projection->project(twoCoordinates.front());
    const auto secondPoint = projection->project(twoCoordinates.back());
    QVERIFY(firstPoint.isVisible);
    QVERIFY(secondPoint.isVisible);

    const double segmentLengthSquared = skygate::core::squaredDistance2d(
        firstPoint.x,
        firstPoint.y,
        secondPoint.x,
        secondPoint.y
    );
    QCOMPARE(builder.build(*projection, twoCoordinates, segmentLengthSquared).size(), 1U);
    QVERIFY(builder.build(*projection, twoCoordinates, -1.0).empty());

    const std::vector<skygate::core::HorizontalCoordinate> coordinatesWithGaps {
        {.altitudeDeg = std::numeric_limits<double>::quiet_NaN(), .azimuthDeg = 180.0},
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        {.altitudeDeg = std::numeric_limits<double>::quiet_NaN(), .azimuthDeg = 180.0},
        {.altitudeDeg = 46.0, .azimuthDeg = 181.0},
    };
    QVERIFY(builder.build(*projection, coordinatesWithGaps, 1'000'000.0).empty());
}

void GeometryHelpersTests::dashedLineBuilderCreatesDashSegments()
{
    const skygate::core::DashedLineBuilder builder;
    const auto segments = builder.build(
        skygate::core::LineSegment2d {.x1 = 0.0, .y1 = 0.0, .x2 = 10.0, .y2 = 0.0},
        3.0,
        2.0
    );

    QCOMPARE(segments.size(), 2U);
    QCOMPARE(segments.front().x1, 0.0);
    QCOMPARE(segments.front().x2, 3.0);
    QCOMPARE(segments.back().x1, 5.0);
    QCOMPARE(segments.back().x2, 8.0);

    QVERIFY(builder.build(
        skygate::core::LineSegment2d {.x1 = 0.0, .y1 = 0.0, .x2 = 0.0, .y2 = 0.0},
        3.0,
        2.0
    ).empty());
}

void GeometryHelpersTests::dashedLineBuilderHandlesDegeneratePatternsAndDiagonals()
{
    const skygate::core::DashedLineBuilder builder;
    const auto contiguousSegments = builder.build(
        skygate::core::LineSegment2d {.x1 = 0.0, .y1 = 0.0, .x2 = 10.0, .y2 = 0.0},
        3.0,
        0.0
    );
    QCOMPARE(contiguousSegments.size(), 4U);
    QCOMPARE(contiguousSegments.back().x1, 9.0);
    QCOMPARE(contiguousSegments.back().x2, 10.0);

    QVERIFY(builder.build(
        skygate::core::LineSegment2d {.x1 = 0.0, .y1 = 0.0, .x2 = 10.0, .y2 = 0.0},
        0.0,
        1.0
    ).empty());
    QVERIFY(builder.build(
        skygate::core::LineSegment2d {.x1 = 0.0, .y1 = 0.0, .x2 = 10.0, .y2 = 0.0},
        3.0,
        -1.0
    ).empty());

    const auto diagonalSegments = builder.build(
        skygate::core::LineSegment2d {.x1 = 0.0, .y1 = 0.0, .x2 = 3.0, .y2 = 4.0},
        2.0,
        1.0
    );
    QCOMPARE(diagonalSegments.size(), 2U);
    QVERIFY(isNear(diagonalSegments.front().x2, 1.2));
    QVERIFY(isNear(diagonalSegments.front().y2, 1.6));
    QVERIFY(isNear(diagonalSegments.back().x1, 1.8));
    QVERIFY(isNear(diagonalSegments.back().y1, 2.4));
    QCOMPARE(diagonalSegments.back().x2, 3.0);
    QCOMPARE(diagonalSegments.back().y2, 4.0);
}

void GeometryHelpersTests::coordinateFiniteQueriesDoNotChangeValidity()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QVERIFY((skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 1.0,
        .declinationDeg = 2.0
    }).isFinite());
    QVERIFY(!(skygate::core::EquatorialCoordinate {
        .rightAscensionHours = nan,
        .declinationDeg = 2.0
    }).isFinite());

    const skygate::core::HorizontalCoordinate validHorizontal {
        .altitudeDeg = 10.0,
        .azimuthDeg = 20.0
    };
    QVERIFY(validHorizontal.isFinite());
    QVERIFY(validHorizontal.isValid());

    const skygate::core::HorizontalCoordinate outOfRangeHorizontal {
        .altitudeDeg = 120.0,
        .azimuthDeg = 20.0
    };
    QVERIFY(outOfRangeHorizontal.isFinite());
    QVERIFY(!outOfRangeHorizontal.isValid());

    QVERIFY(!(skygate::core::HorizontalCoordinate {
        .altitudeDeg = nan,
        .azimuthDeg = 20.0
    }).isFinite());

    QVERIFY((skygate::core::ScreenPoint {.x = 1.0, .y = 2.0}).isFinite());
    QVERIFY(!(skygate::core::ScreenPoint {.x = nan, .y = 2.0}).isFinite());
}

QTEST_APPLESS_MAIN(GeometryHelpersTests)

#include "GeometryHelpersTests.moc"
