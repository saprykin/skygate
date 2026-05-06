#include <QtTest/QtTest>

#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"
#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

class CelestialReferenceCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void computesFiniteEclipticAndEquatorialPoints();
    void declinationCircleFallsBackForNonPositiveSampleCounts();
    void circumpolarBoundaryDeclinationFollowsHemisphere();
    void constellationLabelCenterAveragesAnchorVectors();
};

void CelestialReferenceCalculatorTests::computesFiniteEclipticAndEquatorialPoints()
{
    skygate::core::GeoLocation observer;
    observer.latitudeDeg = 47.3769;
    observer.longitudeDeg = 8.5417;
    observer.elevationMeters = 408.0;
    const skygate::core::UtcTimePoint utcTime(std::chrono::seconds(1'717'276'800));

    const auto eclipticPoint =
        skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(
            90.0,
            observer,
            utcTime
        );
    QVERIFY(std::isfinite(eclipticPoint.altitudeDeg));
    QVERIFY(std::isfinite(eclipticPoint.azimuthDeg));
    QVERIFY(eclipticPoint.altitudeDeg >= -90.0);
    QVERIFY(eclipticPoint.altitudeDeg <= 90.0);
    QVERIFY(eclipticPoint.azimuthDeg >= 0.0);
    QVERIFY(eclipticPoint.azimuthDeg < 360.0);

    const auto equatorialPoint =
        skygate::ephemeris::CelestialReferenceCalculator::equatorialPoint(
            6.0,
            0.0,
            observer,
            utcTime
        );
    QVERIFY(std::isfinite(equatorialPoint.altitudeDeg));
    QVERIFY(std::isfinite(equatorialPoint.azimuthDeg));
    QVERIFY(equatorialPoint.altitudeDeg >= -90.0);
    QVERIFY(equatorialPoint.altitudeDeg <= 90.0);
    QVERIFY(equatorialPoint.azimuthDeg >= 0.0);
    QVERIFY(equatorialPoint.azimuthDeg < 360.0);
}

void CelestialReferenceCalculatorTests::declinationCircleFallsBackForNonPositiveSampleCounts()
{
    skygate::core::GeoLocation observer;
    observer.latitudeDeg = 47.3769;
    observer.longitudeDeg = 8.5417;
    observer.elevationMeters = 408.0;
    const skygate::core::UtcTimePoint utcTime(std::chrono::seconds(1'717'276'800));

    const auto fallbackPoint =
        skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
            5,
            0,
            12.0,
            observer,
            utcTime
        );
    const auto equivalentPoint =
        skygate::ephemeris::CelestialReferenceCalculator::equatorialPoint(
            0.0,
            12.0,
            observer,
            utcTime
        );

    QVERIFY(std::abs(fallbackPoint.altitudeDeg - equivalentPoint.altitudeDeg) < 1e-9);
    QVERIFY(std::abs(fallbackPoint.azimuthDeg - equivalentPoint.azimuthDeg) < 1e-9);
}

void CelestialReferenceCalculatorTests::circumpolarBoundaryDeclinationFollowsHemisphere()
{
    skygate::core::GeoLocation northernObserver;
    northernObserver.latitudeDeg = 47.0;
    QCOMPARE(
        skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
            northernObserver
        ),
        43.0
    );

    skygate::core::GeoLocation southernObserver;
    southernObserver.latitudeDeg = -33.0;
    QCOMPARE(
        skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
            southernObserver
        ),
        -57.0
    );
}

void CelestialReferenceCalculatorTests::constellationLabelCenterAveragesAnchorVectors()
{
    auto bodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
        std::vector<skygate::ephemeris::CelestialBody> {
            skygate::ephemeris::CelestialBody {.id = "hip_1", .displayName = "HIP 1"},
            skygate::ephemeris::CelestialBody {.id = "hip_2", .displayName = "HIP 2"}
        }
    );

    skygate::ephemeris::SkySnapshot snapshot;
    snapshot.catalogBodies = bodies;
    snapshot.states = {
        skygate::ephemeris::CelestialBodyState {
            .bodyIndex = 0,
            .equatorial = {},
            .horizontal = {.altitudeDeg = 0.0, .azimuthDeg = 0.0}
        },
        skygate::ephemeris::CelestialBodyState {
            .bodyIndex = 1,
            .equatorial = {},
            .horizontal = {.altitudeDeg = 0.0, .azimuthDeg = 90.0}
        }
    };

    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs {
        {"Demo", {"HIP_1", "hip_2"}}
    };

    const auto center = skygate::ephemeris::ConstellationReferenceCalculator::labelCenter(
        snapshot,
        labelRefs,
        " demo "
    );
    QVERIFY(center.has_value());
    QVERIFY(std::abs(center->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg - 45.0) < 1e-9);
}

QTEST_GUILESS_MAIN(CelestialReferenceCalculatorTests)

#include "CelestialReferenceCalculatorTests.moc"
