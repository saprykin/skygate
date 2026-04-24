#include "skygate/core/math/ViewportMath.hpp"

#include <QtTest/QtTest>

class ViewportMathTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizeAzimuthWrapsAngles();
    void clampAltitudeEnforcesRange();
    void clampFieldOfViewEnforcesRange();
    void buildProjectionParamsNormalizesAndClampsInputs();
};

void ViewportMathTests::normalizeAzimuthWrapsAngles()
{
    QCOMPARE(skygate::core::ViewportMath::normalizeAzimuthDeg(390.0), 30.0);
    QCOMPARE(skygate::core::ViewportMath::normalizeAzimuthDeg(-30.0), 330.0);
}

void ViewportMathTests::clampAltitudeEnforcesRange()
{
    QCOMPARE(skygate::core::ViewportMath::clampAltitudeDeg(0.0), 0.0);
    QCOMPARE(skygate::core::ViewportMath::clampAltitudeDeg(120.0), 90.0);
    QCOMPARE(skygate::core::ViewportMath::clampAltitudeDeg(-120.0), -90.0);
}

void ViewportMathTests::clampFieldOfViewEnforcesRange()
{
    QCOMPARE(skygate::core::ViewportMath::clampFieldOfViewDeg(90.0), 90.0);
    QCOMPARE(skygate::core::ViewportMath::clampFieldOfViewDeg(0.5), 1.0);
    QCOMPARE(skygate::core::ViewportMath::clampFieldOfViewDeg(170.0), 150.0);
}

void ViewportMathTests::buildProjectionParamsNormalizesAndClampsInputs()
{
    const skygate::core::ProjectionParams params = skygate::core::ViewportMath::buildProjectionParams(
        1200.0,
        800.0,
        120.0,
        -150.0,
        0.5
    );

    QCOMPARE(params.center.altitudeDeg, 90.0);
    QCOMPARE(params.center.azimuthDeg, 210.0);
    QCOMPARE(params.fovDeg, 1.0);
    QCOMPARE(params.rollDeg, 0.0);
    QCOMPARE(params.viewportWidth, 1200.0);
    QCOMPARE(params.viewportHeight, 800.0);
}

QTEST_APPLESS_MAIN(ViewportMathTests)

#include "ViewportMathTests.moc"
