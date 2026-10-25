#include "PreparedProjection.hpp"
#include "projections/ProjectionAlgorithms.hpp"

#include <QtTest/QtTest>

class PreparedProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsInvalidProjectionParams();
    void normalizesStoredCenterAzimuth();
    void reportsTypeAndStoredParams();
    void matchesDirectInvalidCoordinateStatus();
};

void PreparedProjectionTests::rejectsInvalidProjectionParams()
{
    skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMinDeg - 0.1;
    QVERIFY(
        !skygate::core::PreparedProjection::create(skygate::core::ProjectionType::Stereographic, params).has_value()
    );

    params.fovDeg = 90.0;
    params.viewportWidth = 0.0;
    QVERIFY(!skygate::core::PreparedProjection::create(skygate::core::ProjectionType::Perspective, params).has_value());

    params.viewportWidth = 1280.0;
    params.center.altitudeDeg = 91.0;
    QVERIFY(!skygate::core::PreparedProjection::create(skygate::core::ProjectionType::AzimuthalEquidistant, params)
                 .has_value());
}

void PreparedProjectionTests::normalizesStoredCenterAzimuth()
{
    const auto preparedProjection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ProjectionParams{
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
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 10.0, .azimuthDeg = 20.0},
        .fovDeg = 75.0,
        .rollDeg = 12.5,
        .viewportWidth = 900.0,
        .viewportHeight = 500.0,
    };
    const auto preparedProjection =
        skygate::core::PreparedProjection::create(skygate::core::ProjectionType::Perspective, params);

    QVERIFY(preparedProjection.has_value());
    QCOMPARE(preparedProjection->type(), skygate::core::ProjectionType::Perspective);
    QCOMPARE(preparedProjection->params().fovDeg, 75.0);
    QCOMPARE(preparedProjection->params().rollDeg, 12.5);
    QCOMPARE(preparedProjection->params().viewportWidth, 900.0);
    QCOMPARE(preparedProjection->params().viewportHeight, 500.0);
}

void PreparedProjectionTests::matchesDirectInvalidCoordinateStatus()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 100.0,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };
    const auto preparedProjection =
        skygate::core::PreparedProjection::create(skygate::core::ProjectionType::AzimuthalEquidistant, params);
    QVERIFY(preparedProjection.has_value());

    const skygate::core::HorizontalCoordinate invalidCoordinate{.altitudeDeg = -95.0, .azimuthDeg = 180.0};
    const auto directPoint = skygate::core::ProjectionAlgorithms::project(
        skygate::core::ProjectionType::AzimuthalEquidistant, invalidCoordinate, params
    );
    const auto preparedPoint = preparedProjection->project(invalidCoordinate);

    QCOMPARE(preparedPoint.status, directPoint.status);
    QCOMPARE(preparedPoint.isVisible(), directPoint.isVisible());
}

QTEST_APPLESS_MAIN(PreparedProjectionTests)

#include "PreparedProjectionTests.moc"
