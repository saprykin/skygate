#include "skygate/core/ProjectionTypes.hpp"
#include "skygate/core/SkyTypes.hpp"

#include <QtTest/QtTest>

class CoreTypesTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultSkyContextStartsAtEpoch();
    void horizontalCoordinateNormalizesAzimuth();
    void projectionParamsUseCoreProjectionPolicy();
};

void CoreTypesTests::defaultSkyContextStartsAtEpoch()
{
    const skygate::core::SkyContext context;
    QCOMPARE(static_cast<qint64>(context.utcTime.time_since_epoch().count()), 0);
}

void CoreTypesTests::horizontalCoordinateNormalizesAzimuth()
{
    const skygate::core::HorizontalCoordinate coordinate {
        .altitudeDeg = 15.0,
        .azimuthDeg = -30.0,
    };

    QVERIFY(coordinate.isValid());

    const auto normalized = coordinate.normalizedAzimuth();
    QCOMPARE(normalized.altitudeDeg, 15.0);
    QCOMPARE(normalized.azimuthDeg, 330.0);
}

void CoreTypesTests::projectionParamsUseCoreProjectionPolicy()
{
    skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 360.0},
        .fovDeg = skygate::core::ProjectionParams::kFieldOfViewMinDeg,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };

    QVERIFY(params.isProjectable());

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMinDeg - 0.1;
    QVERIFY(!params.isProjectable());

    params.fovDeg = skygate::core::ProjectionParams::kFieldOfViewMaxDeg + 0.1;
    QVERIFY(!params.isProjectable());

    params = {
        .center = {.altitudeDeg = 91.0, .azimuthDeg = 0.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };
    QVERIFY(!params.isProjectable());
}

QTEST_APPLESS_MAIN(CoreTypesTests)

#include "CoreTypesTests.moc"
