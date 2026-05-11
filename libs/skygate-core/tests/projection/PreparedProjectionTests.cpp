#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionFactory.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

}  // namespace

class PreparedProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void matchesDirectProjectionForAllProjectionTypes();
    void rejectsInvalidProjectionParams();
    void normalizesStoredCenterAzimuth();
    void reportsTypeAndStoredParams();
    void matchesDirectInvalidCoordinateStatus();
};

void PreparedProjectionTests::matchesDirectProjectionForAllProjectionTypes()
{
    const std::array<skygate::core::ProjectionType, 3> projectionTypes {
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ProjectionType::AzimuthalEquidistant,
        skygate::core::ProjectionType::Perspective
    };
    const std::array<skygate::core::HorizontalCoordinate, 5> sampleCoordinates {{
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        {.altitudeDeg = 60.0, .azimuthDeg = 150.0},
        {.altitudeDeg = 25.0, .azimuthDeg = 210.0},
        {.altitudeDeg = 5.0, .azimuthDeg = 90.0},
        {.altitudeDeg = -15.0, .azimuthDeg = 310.0},
    }};

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 100.0,
        .rollDeg = 12.5,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };

    for (const auto projectionType : projectionTypes) {
        const auto projection = skygate::core::createProjection(projectionType);
        QVERIFY(projection != nullptr);

        const auto preparedProjection =
            skygate::core::PreparedProjection::create(projectionType, params);
        QVERIFY(preparedProjection.has_value());

        for (const auto& coordinate : sampleCoordinates) {
            const auto directPoint = projection->project(coordinate, params);
            const auto preparedPoint = preparedProjection->project(coordinate);

            QCOMPARE(preparedPoint.status, directPoint.status);
            QCOMPARE(preparedPoint.isVisible, directPoint.isVisible);
            QVERIFY(isNear(preparedPoint.x, directPoint.x, 1e-6));
            QVERIFY(isNear(preparedPoint.y, directPoint.y, 1e-6));
        }
    }
}

void PreparedProjectionTests::rejectsInvalidProjectionParams()
{
    skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMinDeg - 0.1;
    QVERIFY(!skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        params
    ).has_value());

    params.fovDeg = 90.0;
    params.viewportWidth = 0.0;
    QVERIFY(!skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Perspective,
        params
    ).has_value());

    params.viewportWidth = 1280.0;
    params.center.altitudeDeg = 91.0;
    QVERIFY(!skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::AzimuthalEquidistant,
        params
    ).has_value());
}

void PreparedProjectionTests::normalizesStoredCenterAzimuth()
{
    const auto preparedProjection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ProjectionParams {
            .center = {.altitudeDeg = 10.0, .azimuthDeg = -45.0},
            .fovDeg = 90.0,
            .rollDeg = 15.0,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0,
        }
    );

    QVERIFY(preparedProjection.has_value());
    QCOMPARE(preparedProjection->params().center.altitudeDeg, 10.0);
    QCOMPARE(preparedProjection->params().center.azimuthDeg, 315.0);
}

void PreparedProjectionTests::reportsTypeAndStoredParams()
{
    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 10.0, .azimuthDeg = 20.0},
        .fovDeg = 75.0,
        .rollDeg = 12.5,
        .viewportWidth = 900.0,
        .viewportHeight = 500.0,
    };
    const auto preparedProjection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Perspective,
        params
    );

    QVERIFY(preparedProjection.has_value());
    QCOMPARE(preparedProjection->type(), skygate::core::ProjectionType::Perspective);
    QCOMPARE(preparedProjection->params().fovDeg, 75.0);
    QCOMPARE(preparedProjection->params().rollDeg, 12.5);
    QCOMPARE(preparedProjection->params().viewportWidth, 900.0);
    QCOMPARE(preparedProjection->params().viewportHeight, 500.0);
}

void PreparedProjectionTests::matchesDirectInvalidCoordinateStatus()
{
    const auto projection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(projection != nullptr);

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 100.0,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };
    const auto preparedProjection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::AzimuthalEquidistant,
        params
    );
    QVERIFY(preparedProjection.has_value());

    const skygate::core::HorizontalCoordinate invalidCoordinate {
        .altitudeDeg = -95.0,
        .azimuthDeg = 180.0
    };
    const auto directPoint = projection->project(invalidCoordinate, params);
    const auto preparedPoint = preparedProjection->project(invalidCoordinate);

    QCOMPARE(preparedPoint.status, directPoint.status);
    QCOMPARE(preparedPoint.isVisible, directPoint.isVisible);
}

QTEST_APPLESS_MAIN(PreparedProjectionTests)

#include "PreparedProjectionTests.moc"
