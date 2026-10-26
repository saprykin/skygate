#include "math/MathConstants.hpp"
#include "math/SphericalGeometry.hpp"
#include "math/Vector3d.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance = 1e-6)
{
    return std::abs(value - expected) <= tolerance;
}

void compareVector(const skygate::core::Vector3d& actual, const skygate::core::Vector3d& expected)
{
    QVERIFY(isNear(actual.x, expected.x));
    QVERIFY(isNear(actual.y, expected.y));
    QVERIFY(isNear(actual.z, expected.z));
}

void compareHorizontalCoordinate(
    const skygate::core::HorizontalCoordinate& actual,
    const double altitudeDeg,
    const double azimuthDeg,
    const double tolerance = 1e-9
)
{
    QVERIFY(isNear(actual.altitudeDeg, altitudeDeg, tolerance));
    QVERIFY(isNear(actual.azimuthDeg, azimuthDeg, tolerance));
}

}  // namespace

class SphericalGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void equatorialDirectionsMapToUnitVectors();
    void equatorialUnitVectorsRoundTrip();
    void equatorialRoundTripPreservesPoles();
    void horizontalCoordinatesMapToUnitVectors();
    void unitVectorsMapToHorizontalCoordinates();
    void measuresSeparationsBetweenDirections();
    void separatesUnitVectorDirections();
    void separatesNearParallelAndWrappedAzimuths();
    void interpolatesHorizontalCoordinatesAtEndpoints();
    void interpolatesAcrossAzimuthWraparound();
    void interpolatesNearParallelDirections();
    void interpolatesAntipodalEquatorialDirections();
    void fallsBackToZenithForNonFiniteDirection();
    void rotatesUnitVectorsAroundAxis();
    void rotationFallsBackToZenithForZeroVector();
    void computesRotationAxisBetweenDirections();
    void computesRotationAngleBetweenDirections();
    void buildsOrthonormalProjectionBasis();
    void projectionBasisCenterMatchesDirection();
    void projectionBasisNormalizesAzimuthAndSupportsNadir();
    void rejectsInvalidProjectionCenterKeepsOutputs();
};

void SphericalGeometryTests::equatorialDirectionsMapToUnitVectors()
{
    compareVector(
        skygate::core::SphericalGeometry::equatorialToUnitVector({.rightAscensionHours = 0.0, .declinationDeg = 0.0}),
        {1.0, 0.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::equatorialToUnitVector({.rightAscensionHours = 6.0, .declinationDeg = 0.0}),
        {0.0, 1.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::equatorialToUnitVector({.rightAscensionHours = 13.5, .declinationDeg = 90.0}),
        {0.0, 0.0, 1.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::equatorialToUnitVector(
            {.rightAscensionHours = 13.5, .declinationDeg = -90.0}
        ),
        {0.0, 0.0, -1.0}
    );
}

void SphericalGeometryTests::equatorialUnitVectorsRoundTrip()
{
    const skygate::core::EquatorialCoordinate pairs[] = {
        {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
        {.rightAscensionHours = 6.0, .declinationDeg = 45.0},
        {.rightAscensionHours = 23.999, .declinationDeg = -12.5},
        {.rightAscensionHours = 17.25, .declinationDeg = -89.5},
    };

    for (const skygate::core::EquatorialCoordinate& pair : pairs) {
        const skygate::core::EquatorialCoordinate roundTrip =
            skygate::core::SphericalGeometry::equatorialFromUnitVector(
                skygate::core::SphericalGeometry::equatorialToUnitVector(pair)
            );
        QVERIFY(isNear(roundTrip.rightAscensionHours, pair.rightAscensionHours, 1e-9));
        QVERIFY(isNear(roundTrip.declinationDeg, pair.declinationDeg, 1e-9));
    }

    // Right ascension wraps at 24h, so both ends name the same direction.
    compareVector(
        skygate::core::SphericalGeometry::equatorialToUnitVector({.rightAscensionHours = 0.0, .declinationDeg = 30.0}),
        skygate::core::SphericalGeometry::equatorialToUnitVector({.rightAscensionHours = 24.0, .declinationDeg = 30.0})
    );
}

void SphericalGeometryTests::equatorialRoundTripPreservesPoles()
{
    for (const double declinationDeg : {90.0, -90.0}) {
        const skygate::core::Vector3d direction = skygate::core::SphericalGeometry::equatorialToUnitVector(
            {.rightAscensionHours = 3.0, .declinationDeg = declinationDeg}
        );
        compareVector(direction, {.x = 0.0, .y = 0.0, .z = declinationDeg > 0.0 ? 1.0 : -1.0});

        const skygate::core::EquatorialCoordinate roundTrip =
            skygate::core::SphericalGeometry::equatorialFromUnitVector(direction);
        QVERIFY(isNear(roundTrip.declinationDeg, declinationDeg, 1e-9));
        QVERIFY(roundTrip.rightAscensionHours >= 0.0);
        QVERIFY(roundTrip.rightAscensionHours < 24.0);
    }
}

void SphericalGeometryTests::horizontalCoordinatesMapToUnitVectors()
{
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({.altitudeDeg = 0.0, .azimuthDeg = 0.0}),
        {0.0, 1.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({.altitudeDeg = 0.0, .azimuthDeg = 90.0}),
        {1.0, 0.0, 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::horizontalToUnitVector({.altitudeDeg = 90.0, .azimuthDeg = 123.0}),
        {0.0, 0.0, 1.0}
    );
}

void SphericalGeometryTests::unitVectorsMapToHorizontalCoordinates()
{
    const skygate::core::HorizontalCoordinate north =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = 0.0, .y = 1.0, .z = 0.0});
    QVERIFY(isNear(north.altitudeDeg, 0.0));
    QVERIFY(isNear(north.azimuthDeg, 0.0));

    const skygate::core::HorizontalCoordinate east =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = 1.0, .y = 0.0, .z = 0.0});
    QVERIFY(isNear(east.altitudeDeg, 0.0));
    QVERIFY(isNear(east.azimuthDeg, 90.0));

    const skygate::core::HorizontalCoordinate west =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = -1.0, .y = 0.0, .z = 0.0});
    QVERIFY(isNear(west.altitudeDeg, 0.0));
    QVERIFY(isNear(west.azimuthDeg, 270.0));

    const skygate::core::HorizontalCoordinate zenith =
        skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = 0.0, .y = 0.0, .z = 1.0});
    QVERIFY(isNear(zenith.altitudeDeg, 90.0));
}

void SphericalGeometryTests::measuresSeparationsBetweenDirections()
{
    const skygate::core::HorizontalCoordinate direction{.altitudeDeg = 30.0, .azimuthDeg = 200.0};
    QVERIFY(isNear(skygate::core::SphericalGeometry::angularSeparationDegrees(direction, direction), 0.0, 1e-5));

    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationDegrees(
            {.altitudeDeg = 0.0, .azimuthDeg = 0.0}, {.altitudeDeg = 0.0, .azimuthDeg = 180.0}
        ),
        180.0
    ));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationDegrees(
            {.altitudeDeg = 45.0, .azimuthDeg = 30.0}, {.altitudeDeg = -45.0, .azimuthDeg = 210.0}
        ),
        180.0,
        1e-4
    ));
}

void SphericalGeometryTests::separatesUnitVectorDirections()
{
    const skygate::core::Vector3d north{.x = 0.0, .y = 1.0, .z = 0.0};
    const skygate::core::Vector3d east{.x = 1.0, .y = 0.0, .z = 0.0};
    const skygate::core::Vector3d south{.x = 0.0, .y = -1.0, .z = 0.0};

    QVERIFY(isNear(skygate::core::SphericalGeometry::angularSeparationRad(north, north), 0.0, 1e-12));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationRad(north, east),
        skygate::core::MathConstants::kPi / 2.0,
        1e-12
    ));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationRad(north, south), skygate::core::MathConstants::kPi, 1e-12
    ));

    // Unnormalized directions with an over-long dot product must be clamped.
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationRad(
            {.x = 0.0, .y = 2.0, .z = 0.0}, {.x = 0.0, .y = 3.0, .z = 0.0}
        ),
        0.0,
        1e-12
    ));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationRad(
            {.x = 0.0, .y = 2.0, .z = 0.0}, {.x = 0.0, .y = -3.0, .z = 0.0}
        ),
        skygate::core::MathConstants::kPi,
        1e-12
    ));
}

void SphericalGeometryTests::separatesNearParallelAndWrappedAzimuths()
{
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationDegrees(
            {.altitudeDeg = 0.0, .azimuthDeg = 0.0}, {.altitudeDeg = 0.0, .azimuthDeg = 0.001}
        ),
        0.001,
        1e-9
    ));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationDegrees(
            {.altitudeDeg = 44.9995, .azimuthDeg = 100.0005}, {.altitudeDeg = 45.0005, .azimuthDeg = 100.0005}
        ),
        0.001,
        1e-9
    ));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationDegrees(
            {.altitudeDeg = 0.0, .azimuthDeg = 359.5}, {.altitudeDeg = 0.0, .azimuthDeg = 0.5}
        ),
        1.0,
        1e-9
    ));
    QVERIFY(isNear(
        skygate::core::SphericalGeometry::angularSeparationDegrees(
            {.altitudeDeg = 10.0, .azimuthDeg = -0.5}, {.altitudeDeg = 10.0, .azimuthDeg = 359.5}
        ),
        0.0,
        1e-5
    ));
}

void SphericalGeometryTests::interpolatesHorizontalCoordinatesAtEndpoints()
{
    const skygate::core::HorizontalCoordinate previous{.altitudeDeg = 20.0, .azimuthDeg = 10.0};
    const skygate::core::HorizontalCoordinate next{.altitudeDeg = 50.0, .azimuthDeg = 300.0};

    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(previous, next, 0.0), 20.0, 10.0
    );
    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(previous, next, 1.0), 50.0, 300.0
    );
    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(previous, previous, 0.5), 20.0, 10.0
    );
}

void SphericalGeometryTests::interpolatesAcrossAzimuthWraparound()
{
    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(
            {.altitudeDeg = 0.0, .azimuthDeg = 350.0}, {.altitudeDeg = 0.0, .azimuthDeg = 20.0}, 0.5
        ),
        0.0,
        5.0
    );
    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(
            {.altitudeDeg = 0.0, .azimuthDeg = 170.0}, {.altitudeDeg = 0.0, .azimuthDeg = 190.0}, 0.5
        ),
        0.0,
        180.0
    );
}

void SphericalGeometryTests::interpolatesNearParallelDirections()
{
    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(
            {.altitudeDeg = 20.0, .azimuthDeg = 10.0}, {.altitudeDeg = 20.001, .azimuthDeg = 10.001}, 0.5
        ),
        20.0005,
        10.0005,
        1e-6
    );
}

void SphericalGeometryTests::interpolatesAntipodalEquatorialDirections()
{
    const skygate::core::HorizontalCoordinate previous{.altitudeDeg = 0.0, .azimuthDeg = 0.0};
    const skygate::core::HorizontalCoordinate next{.altitudeDeg = 0.0, .azimuthDeg = 180.0};
    const skygate::core::HorizontalCoordinate midpoint =
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(previous, next, 0.5);

    QVERIFY(midpoint.isFinite());
    QVERIFY(isNear(midpoint.altitudeDeg, 0.0, 1e-9));
    QVERIFY(isNear(skygate::core::SphericalGeometry::angularSeparationDegrees(previous, midpoint), 90.0, 1e-6));
    QVERIFY(isNear(skygate::core::SphericalGeometry::angularSeparationDegrees(next, midpoint), 90.0, 1e-6));
}

void SphericalGeometryTests::fallsBackToZenithForNonFiniteDirection()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    compareHorizontalCoordinate(
        skygate::core::SphericalGeometry::interpolateHorizontalCoordinate(
            {.altitudeDeg = nan, .azimuthDeg = 0.0}, {.altitudeDeg = 0.0, .azimuthDeg = 90.0}, 0.5
        ),
        90.0,
        0.0
    );
}

void SphericalGeometryTests::rotatesUnitVectorsAroundAxis()
{
    const skygate::core::Vector3d zAxis{.x = 0.0, .y = 0.0, .z = 1.0};
    const skygate::core::Vector3d north{.x = 0.0, .y = 1.0, .z = 0.0};

    compareVector(skygate::core::SphericalGeometry::rotatedAroundAxis(north, zAxis, 0.0), north);
    compareVector(
        skygate::core::SphericalGeometry::rotatedAroundAxis(north, zAxis, skygate::core::MathConstants::kPi / 2.0),
        {.x = -1.0, .y = 0.0, .z = 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::rotatedAroundAxis(north, zAxis, -skygate::core::MathConstants::kPi / 2.0),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::rotatedAroundAxis(north, zAxis, skygate::core::MathConstants::kPi),
        {.x = 0.0, .y = -1.0, .z = 0.0}
    );
    compareVector(
        skygate::core::SphericalGeometry::rotatedAroundAxis(zAxis, zAxis, skygate::core::MathConstants::kPi / 4.0),
        {.x = 0.0, .y = 0.0, .z = 1.0}
    );
}

void SphericalGeometryTests::rotationFallsBackToZenithForZeroVector()
{
    compareVector(
        skygate::core::SphericalGeometry::rotatedAroundAxis(
            {.x = 0.0, .y = 0.0, .z = 0.0}, {.x = 0.0, .y = 0.0, .z = 1.0}, 1.0
        ),
        {.x = 0.0, .y = 0.0, .z = 1.0}
    );
}

void SphericalGeometryTests::computesRotationAxisBetweenDirections()
{
    const skygate::core::Vector3d north{.x = 0.0, .y = 1.0, .z = 0.0};
    const skygate::core::Vector3d east{.x = 1.0, .y = 0.0, .z = 0.0};
    const skygate::core::Vector3d south{.x = 0.0, .y = -1.0, .z = 0.0};
    const skygate::core::Vector3d zenith{.x = 0.0, .y = 0.0, .z = 1.0};
    const skygate::core::Vector3d nadir{.x = 0.0, .y = 0.0, .z = -1.0};

    compareVector(skygate::core::SphericalGeometry::rotationAxisBetween(north, east), {.x = 0.0, .y = 0.0, .z = -1.0});

    // Collinear directions fall back to the polar axis, then to the equinox.
    compareVector(skygate::core::SphericalGeometry::rotationAxisBetween(north, south), {.x = 1.0, .y = 0.0, .z = 0.0});
    compareVector(skygate::core::SphericalGeometry::rotationAxisBetween(zenith, nadir), {.x = 0.0, .y = 1.0, .z = 0.0});

    QVERIFY(isNear(skygate::core::SphericalGeometry::rotationAxisBetween(north, north).length(), 1.0, 1e-12));
}

void SphericalGeometryTests::computesRotationAngleBetweenDirections()
{
    const skygate::core::Vector3d north{.x = 0.0, .y = 1.0, .z = 0.0};
    const skygate::core::Vector3d east{.x = 1.0, .y = 0.0, .z = 0.0};
    const skygate::core::Vector3d south{.x = 0.0, .y = -1.0, .z = 0.0};

    const skygate::core::Vector3d quadrantAxis = skygate::core::SphericalGeometry::rotationAxisBetween(north, east);
    const double quadrantAngle = skygate::core::SphericalGeometry::rotationAngleBetween(north, east, quadrantAxis);
    QVERIFY(isNear(quadrantAngle, skygate::core::MathConstants::kPi / 2.0, 1e-12));
    compareVector(skygate::core::SphericalGeometry::rotatedAroundAxis(north, quadrantAxis, quadrantAngle), east);

    QVERIFY(isNear(
        skygate::core::SphericalGeometry::rotationAngleBetween(
            north, north, skygate::core::SphericalGeometry::rotationAxisBetween(north, north)
        ),
        0.0,
        1e-12
    ));

    // Anti-parallel directions leave no cross product; the turn is a half turn.
    const skygate::core::Vector3d opposedAxis = skygate::core::SphericalGeometry::rotationAxisBetween(north, south);
    const double opposedAngle = skygate::core::SphericalGeometry::rotationAngleBetween(north, south, opposedAxis);
    QVERIFY(isNear(opposedAngle, skygate::core::MathConstants::kPi, 1e-12));
    compareVector(skygate::core::SphericalGeometry::rotatedAroundAxis(north, opposedAxis, opposedAngle), south);
}

void SphericalGeometryTests::buildsOrthonormalProjectionBasis()
{
    skygate::core::Vector3d center;
    skygate::core::Vector3d right;
    skygate::core::Vector3d up;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = 45.0, .azimuthDeg = 180.0}, center, right, up
        )
    );

    QVERIFY(isNear(center.length(), 1.0));
    QVERIFY(isNear(right.length(), 1.0));
    QVERIFY(isNear(up.length(), 1.0));
    QVERIFY(isNear(center.dot(right), 0.0));
    QVERIFY(isNear(center.dot(up), 0.0));
    QVERIFY(isNear(right.dot(up), 0.0));
}

void SphericalGeometryTests::projectionBasisNormalizesAzimuthAndSupportsNadir()
{
    skygate::core::Vector3d negativeAzimuthCenter;
    skygate::core::Vector3d negativeAzimuthRight;
    skygate::core::Vector3d negativeAzimuthUp;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = 0.0, .azimuthDeg = -90.0}, negativeAzimuthCenter, negativeAzimuthRight, negativeAzimuthUp
        )
    );

    skygate::core::Vector3d wrappedAzimuthCenter;
    skygate::core::Vector3d wrappedAzimuthRight;
    skygate::core::Vector3d wrappedAzimuthUp;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = 0.0, .azimuthDeg = 270.0}, wrappedAzimuthCenter, wrappedAzimuthRight, wrappedAzimuthUp
        )
    );

    compareVector(negativeAzimuthCenter, wrappedAzimuthCenter);
    compareVector(negativeAzimuthRight, wrappedAzimuthRight);
    compareVector(negativeAzimuthUp, wrappedAzimuthUp);

    skygate::core::Vector3d nadirCenter;
    skygate::core::Vector3d nadirRight;
    skygate::core::Vector3d nadirUp;
    QVERIFY(
        skygate::core::SphericalGeometry::tryBuildProjectionBasis(
            {.altitudeDeg = -90.0, .azimuthDeg = 123.0}, nadirCenter, nadirRight, nadirUp
        )
    );

    QVERIFY(isNear(nadirCenter.length(), 1.0));
    QVERIFY(isNear(nadirRight.length(), 1.0));
    QVERIFY(isNear(nadirUp.length(), 1.0));
    QVERIFY(isNear(nadirCenter.dot(nadirRight), 0.0));
    QVERIFY(isNear(nadirCenter.dot(nadirUp), 0.0));
    QVERIFY(isNear(nadirRight.dot(nadirUp), 0.0));
}

void SphericalGeometryTests::projectionBasisCenterMatchesDirection()
{
    const skygate::core::HorizontalCoordinate centers[] = {
        {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        {.altitudeDeg = -30.0, .azimuthDeg = -45.0},
        {.altitudeDeg = 89.0, .azimuthDeg = 359.0},
    };

    for (const skygate::core::HorizontalCoordinate& centerCoordinate : centers) {
        skygate::core::Vector3d center;
        skygate::core::Vector3d right;
        skygate::core::Vector3d up;
        QVERIFY(skygate::core::SphericalGeometry::tryBuildProjectionBasis(centerCoordinate, center, right, up));

        const skygate::core::Vector3d expectedCenter =
            skygate::core::SphericalGeometry::horizontalToUnitVector(centerCoordinate.normalizedAzimuth());
        QCOMPARE(center.x, expectedCenter.x);
        QCOMPARE(center.y, expectedCenter.y);
        QCOMPARE(center.z, expectedCenter.z);
    }
}

void SphericalGeometryTests::rejectsInvalidProjectionCenterKeepsOutputs()
{
    skygate::core::Vector3d center{.x = 1.0, .y = 2.0, .z = 3.0};
    skygate::core::Vector3d right{.x = 4.0, .y = 5.0, .z = 6.0};
    skygate::core::Vector3d up{.x = 7.0, .y = 8.0, .z = 9.0};
    QVERIFY(!skygate::core::SphericalGeometry::tryBuildProjectionBasis(
        {.altitudeDeg = 120.0, .azimuthDeg = 0.0}, center, right, up
    ));

    compareVector(center, {1.0, 2.0, 3.0});
    compareVector(right, {4.0, 5.0, 6.0});
    compareVector(up, {7.0, 8.0, 9.0});
}

QTEST_APPLESS_MAIN(SphericalGeometryTests)

#include "SphericalGeometryTests.moc"
