#include "engine/highprecision/StarAstrometryCalculator.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <optional>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] AstronomicalEpoch epochForYearOffset(const double years) noexcept
{
    return {
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = years * 365.25,
        .timeScale = TimeScale::Tt,
    };
}

[[nodiscard]] EphemerisRequest makeRequest(const EphemerisCorrectionFlags flags, const double years)
{
    EphemerisRequest request;
    request.epoch = epochForYearOffset(years);
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = flags;
    return request;
}

[[nodiscard]] CelestialBody makeAstrometricStar()
{
    CelestialBody body;
    body.id = "test-star";
    body.displayName = "Test Star";
    body.type = CelestialBodyType::Star;
    body.ephemerisSource = CelestialBodyEphemerisSource::Star;
    body.fixedEquatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 10.0,
        .declinationDeg = 20.0,
    };
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *body.fixedEquatorial,
        .referenceEpoch = epochForYearOffset(0.0),
        .properMotionRightAscensionMasPerYear = 15'000.0,
        .properMotionDeclinationMasPerYear = -7'200.0,
        .stellarParallaxMas = 100.0,
        .radialVelocityKmPerSecond = 0.0,
    };
    return body;
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const CelestialBody& body, const EphemerisRequest& request)
{
    return {
        .request = request,
        .body = body,
        .bodyIndex = 0U,
    };
}

[[nodiscard]] double angularDifferenceDegrees(const double lhs, const double rhs) noexcept
{
    double difference = std::fmod(lhs - rhs + 540.0, 360.0) - 180.0;
    if (difference < -180.0) {
        difference += 360.0;
    }
    return std::abs(difference);
}

void compareCoordinates(
    const core::EquatorialCoordinate& actual, const core::EquatorialCoordinate& expected, const double toleranceDegrees
)
{
    QVERIFY(
        angularDifferenceDegrees(actual.rightAscensionHours * 15.0, expected.rightAscensionHours * 15.0)
        <= toleranceDegrees
    );
    QVERIFY(std::abs(actual.declinationDeg - expected.declinationDeg) <= toleranceDegrees);
}

}  // namespace

class StarAstrometryCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void propagatesFullAstrometryWhenCorrectionsAreEnabled();
    void leavesReferenceCoordinateWhenCorrectionsAreDisabled();
    void degradesPartialAstrometryButAppliesAvailableProperMotion();
    void degradesFixedOnlyStarsWhenAstrometryCorrectionsAreRequested();
    void failsWhenNoCoordinateFallbackExists();
};

void StarAstrometryCalculatorTests::propagatesFullAstrometryWhenCorrectionsAreEnabled()
{
    const CelestialBody body = makeAstrometricStar();
    const EphemerisRequest request = makeRequest(
        EphemerisCorrectionFlags::ProperMotion | EphemerisCorrectionFlags::StellarParallax
            | EphemerisCorrectionFlags::RadialVelocity,
        10.0
    );

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(
        *result.equatorial,
        core::EquatorialCoordinate{
            .rightAscensionHours = 10.0027778,
            .declinationDeg = 19.98,
        },
        0.00005
    );
    QCOMPARE(result.metadata.status, EphemerisResultStatus::Valid);
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::ProperMotion));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::StellarParallax));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::RadialVelocity));
}

void StarAstrometryCalculatorTests::leavesReferenceCoordinateWhenCorrectionsAreDisabled()
{
    const CelestialBody body = makeAstrometricStar();
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::NoCorrections, 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(*result.equatorial, *body.fixedEquatorial, 0.0000001);
    QCOMPARE(result.metadata.status, EphemerisResultStatus::Valid);
    QCOMPARE(result.metadata.appliedCorrections, EphemerisCorrectionFlags::NoCorrections);
}

void StarAstrometryCalculatorTests::degradesPartialAstrometryButAppliesAvailableProperMotion()
{
    CelestialBody body = makeAstrometricStar();
    body.starAstrometry->properMotionDeclinationMasPerYear = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::ProperMotion, 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.equatorial->rightAscensionHours > body.fixedEquatorial->rightAscensionHours);
    QVERIFY(std::abs(result.equatorial->declinationDeg - body.fixedEquatorial->declinationDeg) < 0.00001);
    QCOMPARE(result.metadata.status, EphemerisResultStatus::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::ProperMotion));
}

void StarAstrometryCalculatorTests::degradesFixedOnlyStarsWhenAstrometryCorrectionsAreRequested()
{
    CelestialBody body = makeAstrometricStar();
    body.starAstrometry = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::ProperMotion, 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(*result.equatorial, *body.fixedEquatorial, 0.0000001);
    QCOMPARE(result.metadata.status, EphemerisResultStatus::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

void StarAstrometryCalculatorTests::failsWhenNoCoordinateFallbackExists()
{
    CelestialBody body;
    body.id = "missing-coordinate";
    body.type = CelestialBodyType::Star;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::ProperMotion, 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(!result.equatorial.has_value());
    QCOMPARE(result.metadata.status, EphemerisResultStatus::Failed);
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::ComputationFailed));
}

QTEST_APPLESS_MAIN(StarAstrometryCalculatorTests)

#include "StarAstrometryCalculatorTests.moc"
