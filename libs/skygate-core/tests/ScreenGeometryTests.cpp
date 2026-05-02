#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/math/ScreenGeometry.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

class ScreenGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void rectIntersectionAndViewportFit();
    void rectOccupancyGridDetectsCollisions();
    void circleHitIndexReturnsNearestPayload();
    void projectedPolylineSplitsGapsAndDropsLongJumps();
    void dashedLineBuilderCreatesDashSegments();
    void coordinateFiniteQueriesDoNotChangeValidity();
};

void ScreenGeometryTests::rectIntersectionAndViewportFit()
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

void ScreenGeometryTests::rectOccupancyGridDetectsCollisions()
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

void ScreenGeometryTests::circleHitIndexReturnsNearestPayload()
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

void ScreenGeometryTests::projectedPolylineSplitsGapsAndDropsLongJumps()
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

void ScreenGeometryTests::dashedLineBuilderCreatesDashSegments()
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

void ScreenGeometryTests::coordinateFiniteQueriesDoNotChangeValidity()
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

QTEST_APPLESS_MAIN(ScreenGeometryTests)

#include "ScreenGeometryTests.moc"
