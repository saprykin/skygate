#include "skygate/core/HorizontalCoordinate.hpp"
#include "skygate/core/ProjectionParams.hpp"
#include "skygate/core/SkyContext.hpp"
#include "skygate/core/SystemTimeSource.hpp"
#include "skygate/core/UtcTimeCodec.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdlib>
#include <limits>

class CoreTypesTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultSkyContextStartsAtEpoch();
    void horizontalCoordinateNormalizesAzimuth();
    void horizontalCoordinatePreservesNonFiniteAzimuthDuringNormalization();
    void utcTimeCodecRoundTripsMicroseconds();
    void utcTimeCodecFloorsNegativeEpochSeconds();
    void systemTimeSourceUsesMicrosecondStorage();
    void projectionParamsUseCoreProjectionPolicy();
    void projectionParamsRejectNonFiniteValues();
};

void CoreTypesTests::defaultSkyContextStartsAtEpoch()
{
    const skygate::core::SkyContext context;
    QCOMPARE(static_cast<qint64>(context.utcTime.time_since_epoch().count()), 0);
}

void CoreTypesTests::horizontalCoordinateNormalizesAzimuth()
{
    const skygate::core::HorizontalCoordinate coordinate{
        .altitudeDeg = 15.0,
        .azimuthDeg = -30.0,
    };

    QVERIFY(coordinate.isValid());

    const auto normalized = coordinate.normalizedAzimuth();
    QCOMPARE(normalized.altitudeDeg, 15.0);
    QCOMPARE(normalized.azimuthDeg, 330.0);
}

void CoreTypesTests::horizontalCoordinatePreservesNonFiniteAzimuthDuringNormalization()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const skygate::core::HorizontalCoordinate coordinate{
        .altitudeDeg = 15.0,
        .azimuthDeg = nan,
    };

    const auto normalized = coordinate.normalizedAzimuth();

    QCOMPARE(normalized.altitudeDeg, 15.0);
    QVERIFY(std::isnan(normalized.azimuthDeg));
    QVERIFY(!normalized.isFinite());
    QVERIFY(!normalized.isValid());
}

void CoreTypesTests::utcTimeCodecRoundTripsMicroseconds()
{
    const auto utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(1'717'276'800'123'456LL);

    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(utcTime), 1'717'276'800'123'456LL);
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochSecondsFloor(utcTime), 1'717'276'800LL);
    QCOMPARE(
        skygate::core::UtcTimeCodec::toEpochMicros(skygate::core::UtcTimeCodec::fromEpochSeconds(42)), 42'000'000LL
    );
    QCOMPARE(skygate::core::UtcTimeCodec::secondsSinceEpochDouble(utcTime), 1'717'276'800.123456);
}

void CoreTypesTests::utcTimeCodecFloorsNegativeEpochSeconds()
{
    const auto utcTime = skygate::core::UtcTimeCodec::fromEpochMicros(-1'234'001LL);

    QCOMPARE(skygate::core::UtcTimeCodec::toEpochSecondsFloor(utcTime), -2LL);
}

void CoreTypesTests::systemTimeSourceUsesMicrosecondStorage()
{
    const skygate::core::SystemTimeSource timeSource;
    const auto nowUtc = timeSource.nowUtc();

    const auto micros = skygate::core::UtcTimeCodec::toEpochMicros(nowUtc);
    QVERIFY(std::llabs(micros) > 1'000'000LL);
}

void CoreTypesTests::projectionParamsUseCoreProjectionPolicy()
{
    skygate::core::ProjectionParams params{
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

void CoreTypesTests::projectionParamsRejectNonFiniteValues()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 180.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };

    params.center.altitudeDeg = nan;
    QVERIFY(!params.isProjectable());

    params.center = {.altitudeDeg = 0.0, .azimuthDeg = nan};
    QVERIFY(!params.isProjectable());

    params.center = {.altitudeDeg = 0.0, .azimuthDeg = 180.0};
    params.fovDeg = nan;
    QVERIFY(!params.isProjectable());

    params.fovDeg = 90.0;
    params.rollDeg = infinity;
    QVERIFY(!params.isProjectable());

    params.rollDeg = 0.0;
    params.viewportWidth = nan;
    QVERIFY(!params.isProjectable());

    params.viewportWidth = 1280.0;
    params.viewportHeight = infinity;
    QVERIFY(!params.isProjectable());
}

QTEST_APPLESS_MAIN(CoreTypesTests)

#include "CoreTypesTests.moc"
