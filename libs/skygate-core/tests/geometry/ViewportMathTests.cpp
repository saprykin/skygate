#include "skygate/core/math/ViewportMath.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

class ViewportMathTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizeAzimuthWrapsAngles();
    void clampAltitudeEnforcesRange();
    void clampFieldOfViewEnforcesRange();
    void nonFiniteInputsRemainNonFinite();
    void buildProjectionParamsNormalizesAndClampsInputs();
    void buildProjectionParamsPreservesNonFinitePolicy();
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

void ViewportMathTests::nonFiniteInputsRemainNonFinite()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QVERIFY(std::isnan(skygate::core::ViewportMath::normalizeAzimuthDeg(nan)));
    QVERIFY(std::isnan(skygate::core::ViewportMath::clampAltitudeDeg(nan)));
    QVERIFY(std::isnan(skygate::core::ViewportMath::clampFieldOfViewDeg(nan)));
}

void ViewportMathTests::buildProjectionParamsNormalizesAndClampsInputs()
{
    const skygate::core::ProjectionParams params =
        skygate::core::ViewportMath::buildProjectionParams(
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

void ViewportMathTests::buildProjectionParamsPreservesNonFinitePolicy()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    const skygate::core::ProjectionParams params =
        skygate::core::ViewportMath::buildProjectionParams(
            nan,
            800.0,
            nan,
            nan,
            nan
        );

    QVERIFY(std::isnan(params.center.altitudeDeg));
    QVERIFY(std::isnan(params.center.azimuthDeg));
    QVERIFY(std::isnan(params.fovDeg));
    QVERIFY(std::isnan(params.viewportWidth));
    QCOMPARE(params.viewportHeight, 800.0);
    QVERIFY(!params.isProjectable());
}

QTEST_APPLESS_MAIN(ViewportMathTests)

#include "ViewportMathTests.moc"
