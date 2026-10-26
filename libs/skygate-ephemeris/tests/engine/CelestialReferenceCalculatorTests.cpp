#include "CelestialReferenceCalculator.hpp"
#include "ConstellationReferenceCalculator.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::EphemerisSnapshot
makeAnchorSnapshot(const std::initializer_list<std::pair<std::string, skygate::core::HorizontalCoordinate>> bodies)
{
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> catalogBodies;
    std::vector<skygate::ephemeris::CelestialBodyState> states;
    catalogBodies.reserve(bodies.size());
    states.reserve(bodies.size());
    std::uint32_t bodyIndex = 0U;
    for (const auto& [id, horizontal] : bodies) {
        skygate::ephemeris::OwnGalaxyCelestialBody body;
        body.id = id;
        body.displayName = id;
        catalogBodies.push_back(std::move(body));
        states.push_back(skygate::ephemeris::CelestialBodyState{.bodyIndex = bodyIndex, .horizontal = horizontal});
        ++bodyIndex;
    }

    skygate::ephemeris::EphemerisSnapshot snapshot;
    snapshot.catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(std::move(catalogBodies));
    snapshot.states = std::move(states);
    return snapshot;
}

}  // namespace

class CelestialReferenceCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void computesFiniteEclipticAndEquatorialPoints();
    void eclipticCircleMapsIndexToLongitude();
    void declinationCircleFallsBackForNonPositiveCounts();
    void circumpolarBoundaryDeclinationFollowsHemisphere();
    void constellationAnchorCentroidAveragesAnchorVectors();
    void constellationAnchorCentroidSkipsMissingAnchors();
    void constellationAnchorCentroidSkipsNonFiniteAnchors();
    void constellationAnchorCentroidCountsDuplicateIds();
    void constellationAnchorCentroidKeepsFirstBodyState();
    void constellationAnchorCentroidKeepsCancelResidue();
};

void CelestialReferenceCalculatorTests::computesFiniteEclipticAndEquatorialPoints()
{
    skygate::core::GeoLocation observer;
    observer.latitudeDeg = 47.3769;
    observer.longitudeDeg = 8.5417;
    observer.elevationMeters = 408.0;
    const skygate::core::UtcTimePoint utcTime(std::chrono::seconds(1'717'276'800));

    const auto eclipticPoint = skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(90.0, observer, utcTime);
    QVERIFY(std::isfinite(eclipticPoint.altitudeDeg));
    QVERIFY(std::isfinite(eclipticPoint.azimuthDeg));
    QVERIFY(eclipticPoint.altitudeDeg >= -90.0);
    QVERIFY(eclipticPoint.altitudeDeg <= 90.0);
    QVERIFY(eclipticPoint.azimuthDeg >= 0.0);
    QVERIFY(eclipticPoint.azimuthDeg < 360.0);

    const auto equatorialPoint =
        skygate::ephemeris::CelestialReferenceCalculator::equatorialPoint(6.0, 0.0, observer, utcTime);
    QVERIFY(std::isfinite(equatorialPoint.altitudeDeg));
    QVERIFY(std::isfinite(equatorialPoint.azimuthDeg));
    QVERIFY(equatorialPoint.altitudeDeg >= -90.0);
    QVERIFY(equatorialPoint.altitudeDeg <= 90.0);
    QVERIFY(equatorialPoint.azimuthDeg >= 0.0);
    QVERIFY(equatorialPoint.azimuthDeg < 360.0);
}

void CelestialReferenceCalculatorTests::eclipticCircleMapsIndexToLongitude()
{
    skygate::core::GeoLocation observer;
    observer.latitudeDeg = 47.3769;
    observer.longitudeDeg = 8.5417;
    observer.elevationMeters = 408.0;
    const skygate::core::UtcTimePoint utcTime(std::chrono::seconds(1'717'276'800));
    constexpr int kSampleCount = skygate::ephemeris::CelestialReferenceCalculator::kReferenceCircleSamples;

    const auto atStart =
        skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(0, kSampleCount, observer, utcTime);
    const auto atZeroLongitude =
        skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(0.0, observer, utcTime);
    QCOMPARE(atStart.altitudeDeg, atZeroLongitude.altitudeDeg);
    QCOMPARE(atStart.azimuthDeg, atZeroLongitude.azimuthDeg);

    const auto atQuarter = skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(
        kSampleCount / 4, kSampleCount, observer, utcTime
    );
    const auto atNinetyDegrees =
        skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(90.0, observer, utcTime);
    QCOMPARE(atQuarter.altitudeDeg, atNinetyDegrees.altitudeDeg);
    QCOMPARE(atQuarter.azimuthDeg, atNinetyDegrees.azimuthDeg);

    const auto fullTurn = skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(
        kSampleCount, kSampleCount, observer, utcTime
    );
    QCOMPARE(fullTurn.altitudeDeg, atZeroLongitude.altitudeDeg);
    QCOMPARE(fullTurn.azimuthDeg, atZeroLongitude.azimuthDeg);

    const auto fallback =
        skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(3, 0, observer, utcTime);
    QCOMPARE(fallback.altitudeDeg, atZeroLongitude.altitudeDeg);
    QCOMPARE(fallback.azimuthDeg, atZeroLongitude.azimuthDeg);
}

void CelestialReferenceCalculatorTests::declinationCircleFallsBackForNonPositiveCounts()
{
    skygate::core::GeoLocation observer;
    observer.latitudeDeg = 47.3769;
    observer.longitudeDeg = 8.5417;
    observer.elevationMeters = 408.0;
    const skygate::core::UtcTimePoint utcTime(std::chrono::seconds(1'717'276'800));

    const auto fallbackPoint =
        skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(5, 0, 12.0, observer, utcTime);
    const auto equivalentPoint =
        skygate::ephemeris::CelestialReferenceCalculator::equatorialPoint(0.0, 12.0, observer, utcTime);

    QVERIFY(std::abs(fallbackPoint.altitudeDeg - equivalentPoint.altitudeDeg) < 1e-9);
    QVERIFY(std::abs(fallbackPoint.azimuthDeg - equivalentPoint.azimuthDeg) < 1e-9);
}

void CelestialReferenceCalculatorTests::circumpolarBoundaryDeclinationFollowsHemisphere()
{
    skygate::core::GeoLocation northernObserver;
    northernObserver.latitudeDeg = 47.0;
    QCOMPARE(
        skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(northernObserver), 43.0
    );

    skygate::core::GeoLocation southernObserver;
    southernObserver.latitudeDeg = -33.0;
    QCOMPARE(
        skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(southernObserver), -57.0
    );
}

void CelestialReferenceCalculatorTests::constellationAnchorCentroidAveragesAnchorVectors()
{
    skygate::ephemeris::OwnGalaxyCelestialBody firstBody;
    firstBody.id = "hip_1";
    firstBody.displayName = "HIP 1";
    skygate::ephemeris::OwnGalaxyCelestialBody secondBody;
    secondBody.id = "hip_2";
    secondBody.displayName = "HIP 2";
    auto bodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{firstBody, secondBody}
    );

    skygate::ephemeris::EphemerisSnapshot snapshot;
    snapshot.catalogBodies = bodies;
    snapshot.states = {
        skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0, .equatorial = {}, .horizontal = {.altitudeDeg = 0.0, .azimuthDeg = 0.0}
        },
        skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 1, .equatorial = {}, .horizontal = {.altitudeDeg = 0.0, .azimuthDeg = 90.0}
        }
    };

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups{{"Demo", {"HIP_1", "hip_2"}}};

    const auto center =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, anchorGroups, " demo ");
    QVERIFY(center.has_value());
    QVERIFY(std::abs(center->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg - 45.0) < 1e-9);
}

void CelestialReferenceCalculatorTests::constellationAnchorCentroidSkipsMissingAnchors()
{
    const auto snapshot = makeAnchorSnapshot({{"hip_1", {.altitudeDeg = 0.0, .azimuthDeg = 90.0}}});

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> withAbsentAnchor{{"Demo", {"hip_lost", " HIP_1 "}}};
    const auto center =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, withAbsentAnchor, "demo");
    QVERIFY(center.has_value());
    QVERIFY(std::abs(center->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg - 90.0) < 1e-9);

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> absentOnly{{"Demo", {"hip_lost"}}};
    QVERIFY(
        !skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, absentOnly, "demo").has_value()
    );

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> withoutAnchors{{"Demo", {}}};
    QVERIFY(!skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, withoutAnchors, "demo")
                 .has_value());
}

void CelestialReferenceCalculatorTests::constellationAnchorCentroidSkipsNonFiniteAnchors()
{
    const double notANumber = std::numeric_limits<double>::quiet_NaN();
    const auto snapshot = makeAnchorSnapshot({
        {"hip_1", {.altitudeDeg = 0.0, .azimuthDeg = 90.0}},
        {"hip_2", {.altitudeDeg = notANumber, .azimuthDeg = notANumber}},
    });

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> mixedAnchors{{"Demo", {"hip_1", "hip_2"}}};
    const auto center =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, mixedAnchors, "demo");
    QVERIFY(center.has_value());
    QVERIFY(std::abs(center->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg - 90.0) < 1e-9);

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> nonFiniteOnly{{"Demo", {"hip_2"}}};
    QVERIFY(!skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, nonFiniteOnly, "demo")
                 .has_value());
}

void CelestialReferenceCalculatorTests::constellationAnchorCentroidCountsDuplicateIds()
{
    const auto snapshot = makeAnchorSnapshot({{"hip_1", {.altitudeDeg = 0.0, .azimuthDeg = 0.0}}});

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> singleAnchor{{"Demo", {"hip_1"}}};
    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> duplicateAnchors{{"Demo", {"hip_1", " HIP_1 "}}};
    const auto center =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, duplicateAnchors, "demo");
    const auto singleCenter =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, singleAnchor, "demo");

    QVERIFY(center.has_value());
    QVERIFY(singleCenter.has_value());
    QVERIFY(std::abs(center->altitudeDeg - singleCenter->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg - singleCenter->azimuthDeg) < 1e-9);
}

void CelestialReferenceCalculatorTests::constellationAnchorCentroidKeepsFirstBodyState()
{
    const auto snapshot = makeAnchorSnapshot({
        {"hip_1", {.altitudeDeg = 0.0, .azimuthDeg = 0.0}},
        {" HIP_1 ", {.altitudeDeg = 0.0, .azimuthDeg = 90.0}},
    });

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups{{"Demo", {"hip_1"}}};
    const auto center =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, anchorGroups, "DEMO");
    QVERIFY(center.has_value());
    QVERIFY(std::abs(center->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg) < 1e-9);
}

void CelestialReferenceCalculatorTests::constellationAnchorCentroidKeepsCancelResidue()
{
    const auto snapshot = makeAnchorSnapshot({
        {"hip_1", {.altitudeDeg = 0.0, .azimuthDeg = 0.0}},
        {"hip_2", {.altitudeDeg = 0.0, .azimuthDeg = 180.0}},
    });

    const std::vector<skygate::ephemeris::ConstellationAnchorGroup> oppositeAnchors{{"Demo", {"hip_1", "hip_2"}}};
    const auto center =
        skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(snapshot, oppositeAnchors, "demo");

    // Opposite horizon anchors leave a floating-point residue that still
    // normalizes, so the centroid stays defined instead of collapsing.
    QVERIFY(center.has_value());
    QVERIFY(std::abs(center->altitudeDeg) < 1e-9);
    QVERIFY(std::abs(center->azimuthDeg - 90.0) < 1e-6);
}

QTEST_GUILESS_MAIN(CelestialReferenceCalculatorTests)

#include "CelestialReferenceCalculatorTests.moc"
