#include "CelestialBodyCatalog.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "TestCalcephKernel.hpp"
#include "math/MathConstants.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

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
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(flags);
    return request;
}

[[nodiscard]] OwnGalaxyCelestialBody makeAstrometricStar()
{
    OwnGalaxyCelestialBody body;
    body.id = "test-star";
    body.displayName = "Test Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{
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

[[nodiscard]] OwnGalaxyCelestialBody makePartialAstrometricStar()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.id = "partial-star";
    body.displayName = "Partial Star";
    body.starAstrometry->properMotionDeclinationMasPerYear = std::nullopt;
    body.starAstrometry->stellarParallaxMas = std::nullopt;
    body.starAstrometry->radialVelocityKmPerSecond = std::nullopt;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeFixedOnlyStar()
{
    OwnGalaxyCelestialBody body;
    body.id = "fixed-star";
    body.displayName = "Fixed Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = -15.0,
    };
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeStarWithInvalidOptionalAstrometry()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.id = "invalid-optional-star";
    body.displayName = "Invalid Optional Star";
    body.starAstrometry->properMotionRightAscensionMasPerYear = std::numeric_limits<double>::quiet_NaN();
    body.starAstrometry->properMotionDeclinationMasPerYear = std::numeric_limits<double>::infinity();
    body.starAstrometry->stellarParallaxMas = std::numeric_limits<double>::quiet_NaN();
    body.starAstrometry->radialVelocityKmPerSecond = -std::numeric_limits<double>::infinity();
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeStarWithZeroParallaxAstrometry()
{
    OwnGalaxyCelestialBody body = makeStarWithInvalidOptionalAstrometry();
    body.id = "zero-parallax-star";
    body.displayName = "Zero Parallax Star";
    body.starAstrometry->stellarParallaxMas = 0.0;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makePlanet()
{
    OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] CelestialBodyCatalog makeCatalog(const std::vector<OwnGalaxyCelestialBody>& bodies)
{
    return CelestialBodyCatalog(std::span<const OwnGalaxyCelestialBody>{bodies});
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const BaseCelestialBody& body, const EphemerisRequest& request)
{
    return {
        .request = request,
        .body = body,
        .bodyIndex = 0U,
    };
}

[[nodiscard]] HighPrecisionComputationInput
makeInput(const OwnGalaxyCelestialBody& body, const EphemerisRequest& request)
{
    static thread_local CelestialBodyCatalog catalog;
    catalog = CelestialBodyCatalog(std::vector<OwnGalaxyCelestialBody>{body});
    return makeInput(catalog.bodyAt(0), request);
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
    const EquatorialCoordinate& actual, const EquatorialCoordinate& expected, const double toleranceDegrees
)
{
    QVERIFY(
        angularDifferenceDegrees(actual.rightAscensionHours * 15.0, expected.rightAscensionHours * 15.0)
        <= toleranceDegrees
    );
    QVERIFY(std::abs(actual.declinationDeg - expected.declinationDeg) <= toleranceDegrees);
}

[[nodiscard]] double angularSeparationDegrees(const EquatorialCoordinate& lhs, const EquatorialCoordinate& rhs) noexcept
{
    const double lhsRaRad = lhs.rightAscensionHours * 15.0 * MathConstants::kPi / 180.0;
    const double rhsRaRad = rhs.rightAscensionHours * 15.0 * MathConstants::kPi / 180.0;
    const double lhsDecRad = lhs.declinationDeg * MathConstants::kPi / 180.0;
    const double rhsDecRad = rhs.declinationDeg * MathConstants::kPi / 180.0;
    const double cosine = std::sin(lhsDecRad) * std::sin(rhsDecRad)
                          + std::cos(lhsDecRad) * std::cos(rhsDecRad) * std::cos(lhsRaRad - rhsRaRad);
    return std::acos(std::clamp(cosine, -1.0, 1.0)) * 180.0 / MathConstants::kPi;
}

void compareCalculatorResults(
    const HighPrecisionCalculatorResult& actual, const HighPrecisionCalculatorResult& expected
)
{
    QCOMPARE(actual.equatorial.has_value(), expected.equatorial.has_value());
    if (actual.equatorial.has_value() && expected.equatorial.has_value()) {
        compareCoordinates(*actual.equatorial, *expected.equatorial, 0.0000001);
    }
    QCOMPARE(actual.observerRelativePositionAu.has_value(), expected.observerRelativePositionAu.has_value());
    if (actual.observerRelativePositionAu.has_value() && expected.observerRelativePositionAu.has_value()) {
        QCOMPARE(actual.observerRelativePositionAu->x, expected.observerRelativePositionAu->x);
        QCOMPARE(actual.observerRelativePositionAu->y, expected.observerRelativePositionAu->y);
        QCOMPARE(actual.observerRelativePositionAu->z, expected.observerRelativePositionAu->z);
    }
    QCOMPARE(actual.metadata.status, expected.metadata.status);
    QCOMPARE(actual.metadata.appliedCorrections, expected.metadata.appliedCorrections);
    QCOMPARE(actual.metadata.unavailableCorrections, expected.metadata.unavailableCorrections);
    QCOMPARE(actual.metadata.warningCodeMask, expected.metadata.warningCodeMask);
}

[[nodiscard]] std::shared_ptr<skygate::ephemeris::tests::TestCalcephKernel>
makeFixedEarthKernel(std::optional<Vector3d> earthPositionAu, const bool requireTdbEpoch = false)
{
    SolarSystemKernelStateResult result;
    result.positionAu = earthPositionAu;
    result.metadata.status = earthPositionAu.has_value() ? skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid
                                                         : skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
    result.metadata.dataSourceProvenance = "unit-test Earth barycentric state";
    if (!earthPositionAu.has_value()) {
        result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    }

    auto kernel = std::make_shared<skygate::ephemeris::tests::TestCalcephKernel>();
    kernel->setDefaultResult(std::move(result));
    if (requireTdbEpoch) {
        kernel->setRequiredTimeScale(TimeScale::Tdb);
    }
    return kernel;
}

class FixedTdbTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++m_callCount;
        m_lastTargetScale = targetScale;

        TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);

        TimeScaleConversionResult result;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] TimeScale lastTargetScale() const noexcept
    {
        return m_lastTargetScale;
    }

private:
    mutable int m_callCount = 0;
    mutable TimeScale m_lastTargetScale = TimeScale::Utc;
};

}  // namespace

class StarAstrometryCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void propagatesFullAstrometryWhenCorrectionsAreEnabled();
    void leavesReferenceCoordinateWhenCorrectionsAreDisabled();
    void treatsRightAscensionProperMotionAsTangentPlaneComponent();
    void appliesAnnualParallaxWithEarthBarycentricState();
    void batchMatchesSingleStarPropagationForFullPartialAndFixedStars();
    void batchMatchesSingleStarWhenCorrectionsAreDisabled();
    void batchMatchesSingleStarForInvalidOptionalAstrometry();
    void batchMatchesSingleStarAnnualParallaxCorrections();
    void degradesAnnualParallaxWhenKernelProviderIsMissing();
    void degradesAnnualParallaxWhenSourceParallaxIsMissing();
    void degradesRadialVelocityWhenStellarParallaxIsDisabled();
    void degradesPartialAstrometryAndReportsProperMotionUnavailable();
    void degradesFixedOnlyStarsWhenAstrometryCorrectionsAreRequested();
    void failsWhenNoCoordinateFallbackExists();
};

void StarAstrometryCalculatorTests::propagatesFullAstrometryWhenCorrectionsAreEnabled()
{
    const OwnGalaxyCelestialBody body = makeAstrometricStar();
    const EphemerisRequest request = makeRequest(
        EphemerisCorrectionFlags::properMotion() | EphemerisCorrectionFlags::stellarParallax()
            | EphemerisCorrectionFlags::radialVelocity(),
        10.0
    );

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(
        *result.equatorial,
        EquatorialCoordinate{
            .rightAscensionHours = 10.0029557,
            .declinationDeg = 19.9799945,
        },
        0.00005
    );
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::properMotion()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::stellarParallax()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::radialVelocity()
        )
    );
}

void StarAstrometryCalculatorTests::leavesReferenceCoordinateWhenCorrectionsAreDisabled()
{
    const OwnGalaxyCelestialBody body = makeAstrometricStar();
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::noCorrections(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(*result.equatorial, *body.fixedEquatorial, 0.0000001);
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid);
    QCOMPARE(result.metadata.appliedCorrections, EphemerisCorrectionFlags::noCorrections());
}

void StarAstrometryCalculatorTests::treatsRightAscensionProperMotionAsTangentPlaneComponent()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = 10.0,
        .declinationDeg = 60.0,
    };
    body.starAstrometry->referenceEquatorial = *body.fixedEquatorial;
    body.starAstrometry->properMotionRightAscensionMasPerYear = 18'000.0;
    body.starAstrometry->properMotionDeclinationMasPerYear = 0.0;
    body.starAstrometry->stellarParallaxMas = std::nullopt;
    body.starAstrometry->radialVelocityKmPerSecond = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::properMotion(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(
        *result.equatorial,
        EquatorialCoordinate{
            .rightAscensionHours = 10.0066667,
            .declinationDeg = 59.9999622,
        },
        0.000001
    );
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::properMotion()
        )
    );
}

void StarAstrometryCalculatorTests::appliesAnnualParallaxWithEarthBarycentricState()
{
    const OwnGalaxyCelestialBody body = makeAstrometricStar();
    const EphemerisRequest referenceRequest = makeRequest(EphemerisCorrectionFlags::stellarParallax(), 0.0);
    const EphemerisRequest parallaxRequest = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);
    auto kernel = makeFixedEarthKernel(Vector3d{.x = 0.0, .y = 1.0, .z = 0.0}, true);
    auto timeScaleService = std::make_shared<FixedTdbTimeScaleService>();

    const StarAstrometryCalculator calculator(kernel, timeScaleService);
    const HighPrecisionCalculatorResult referenceResult = calculator.calculate(makeInput(body, referenceRequest));
    const HighPrecisionCalculatorResult parallaxResult = calculator.calculate(makeInput(body, parallaxRequest));

    QVERIFY(referenceResult.equatorial.has_value());
    QVERIFY(parallaxResult.equatorial.has_value());
    QVERIFY(parallaxResult.observerRelativePositionAu.has_value());
    QVERIFY(angularSeparationDegrees(*referenceResult.equatorial, *parallaxResult.equatorial) > 1.0e-6);
    QCOMPARE(parallaxResult.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            parallaxResult.metadata.appliedCorrections, EphemerisCorrectionFlags::annualParallax()
        )
    );
    QCOMPARE(kernel->callCount(), 1);
    QCOMPARE(kernel->lastTargetNaifId(), 399);
    QCOMPARE(kernel->lastCenterNaifId(), 0);
    QCOMPARE(static_cast<std::uint8_t>(kernel->lastEpoch().timeScale), static_cast<std::uint8_t>(TimeScale::Tdb));
    QCOMPARE(timeScaleService->callCount(), 1);
    QCOMPARE(static_cast<std::uint8_t>(timeScaleService->lastTargetScale()), static_cast<std::uint8_t>(TimeScale::Tdb));
}

void StarAstrometryCalculatorTests::batchMatchesSingleStarPropagationForFullPartialAndFixedStars()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makePlanet(),
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };
    const CatalogStarAstrometryArrays arrays(makeCatalog(bodies).bodies());
    const EphemerisRequest request = makeRequest(
        EphemerisCorrectionFlags::properMotion() | EphemerisCorrectionFlags::stellarParallax()
            | EphemerisCorrectionFlags::radialVelocity(),
        10.0
    );

    const StarAstrometryCalculator calculator;
    const std::vector<StarAstrometryBatchResult> batchResults = calculator.calculateBatch(request, arrays);

    QCOMPARE(batchResults.size(), 3U);
    QCOMPARE(batchResults[0].bodyIndex, 1U);
    QCOMPARE(batchResults[1].bodyIndex, 2U);
    QCOMPARE(batchResults[2].bodyIndex, 3U);
    for (const StarAstrometryBatchResult& batchResult : batchResults) {
        const HighPrecisionCalculatorResult singleResult =
            calculator.calculate(makeInput(bodies[batchResult.bodyIndex], request));
        compareCalculatorResults(batchResult.result, singleResult);
    }
}

void StarAstrometryCalculatorTests::batchMatchesSingleStarWhenCorrectionsAreDisabled()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };
    const CatalogStarAstrometryArrays arrays(makeCatalog(bodies).bodies());
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::noCorrections(), 10.0);

    const StarAstrometryCalculator calculator;
    const std::vector<StarAstrometryBatchResult> batchResults = calculator.calculateBatch(request, arrays);

    QCOMPARE(batchResults.size(), 3U);
    for (const StarAstrometryBatchResult& batchResult : batchResults) {
        const HighPrecisionCalculatorResult singleResult =
            calculator.calculate(makeInput(bodies[batchResult.bodyIndex], request));
        compareCalculatorResults(batchResult.result, singleResult);
    }
}

void StarAstrometryCalculatorTests::batchMatchesSingleStarForInvalidOptionalAstrometry()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makeStarWithInvalidOptionalAstrometry(),
        makeStarWithZeroParallaxAstrometry(),
    };
    const CatalogStarAstrometryArrays arrays(makeCatalog(bodies).bodies());
    const EphemerisRequest request = makeRequest(
        EphemerisCorrectionFlags::properMotion() | EphemerisCorrectionFlags::stellarParallax()
            | EphemerisCorrectionFlags::radialVelocity(),
        10.0
    );

    const StarAstrometryCalculator calculator;
    const std::vector<StarAstrometryBatchResult> batchResults = calculator.calculateBatch(request, arrays);

    QCOMPARE(batchResults.size(), 2U);
    QCOMPARE(batchResults[0].bodyIndex, 0U);
    QCOMPARE(batchResults[1].bodyIndex, 1U);
    QCOMPARE(arrays.hasStellarParallaxMask()[0], std::uint8_t{0});
    QCOMPARE(arrays.hasStellarParallaxMask()[1], std::uint8_t{0});
    QCOMPARE(arrays.stellarParallaxMasValues()[1], 0.0);

    for (const StarAstrometryBatchResult& batchResult : batchResults) {
        const HighPrecisionCalculatorResult singleResult =
            calculator.calculate(makeInput(bodies[batchResult.bodyIndex], request));
        compareCalculatorResults(batchResult.result, singleResult);
        QCOMPARE(singleResult.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
        QVERIFY(singleResult.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    }

    const HighPrecisionCalculatorResult singleResult = calculator.calculate(makeInput(bodies[0], request));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            singleResult.metadata.unavailableCorrections, EphemerisCorrectionFlags::properMotion()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            singleResult.metadata.unavailableCorrections, EphemerisCorrectionFlags::stellarParallax()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            singleResult.metadata.unavailableCorrections, EphemerisCorrectionFlags::radialVelocity()
        )
    );
}

void StarAstrometryCalculatorTests::batchMatchesSingleStarAnnualParallaxCorrections()
{
    const std::vector<OwnGalaxyCelestialBody> bodies{
        makeAstrometricStar(),
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };
    const CatalogStarAstrometryArrays arrays(makeCatalog(bodies).bodies());
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);
    auto kernel = makeFixedEarthKernel(Vector3d{.x = 0.0, .y = 1.0, .z = 0.0}, true);
    auto timeScaleService = std::make_shared<FixedTdbTimeScaleService>();

    const StarAstrometryCalculator calculator(kernel, timeScaleService);
    const std::vector<StarAstrometryBatchResult> batchResults = calculator.calculateBatch(request, arrays);

    QCOMPARE(batchResults.size(), 4U);
    QCOMPARE(kernel->callCount(), 1);
    QCOMPARE(timeScaleService->callCount(), 1);
    for (const StarAstrometryBatchResult& batchResult : batchResults) {
        const HighPrecisionCalculatorResult singleResult =
            calculator.calculate(makeInput(bodies[batchResult.bodyIndex], request));
        compareCalculatorResults(batchResult.result, singleResult);
    }
    QCOMPARE(kernel->lastTargetNaifId(), 399);
    QCOMPARE(kernel->lastCenterNaifId(), 0);
}

void StarAstrometryCalculatorTests::degradesAnnualParallaxWhenKernelProviderIsMissing()
{
    const OwnGalaxyCelestialBody body = makeAstrometricStar();
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(!result.observerRelativePositionAu.has_value());
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::annualParallax()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::annualParallax()
    ));
}

void StarAstrometryCalculatorTests::degradesAnnualParallaxWhenSourceParallaxIsMissing()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.starAstrometry->stellarParallaxMas = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);
    auto kernel = makeFixedEarthKernel(Vector3d{.x = 0.0, .y = 1.0, .z = 0.0});

    const StarAstrometryCalculator calculator(kernel);
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(*result.equatorial, *body.fixedEquatorial, 0.0000001);
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::annualParallax()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::annualParallax()
    ));
    QCOMPARE(kernel->callCount(), 0);
}

void StarAstrometryCalculatorTests::degradesRadialVelocityWhenStellarParallaxIsDisabled()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.starAstrometry->properMotionRightAscensionMasPerYear = 0.0;
    body.starAstrometry->properMotionDeclinationMasPerYear = 0.0;
    body.starAstrometry->radialVelocityKmPerSecond = 25.0;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::radialVelocity(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(*result.equatorial, *body.fixedEquatorial, 0.0000001);
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::radialVelocity()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::radialVelocity()
    ));
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::stellarParallax()
    ));
}

void StarAstrometryCalculatorTests::degradesPartialAstrometryAndReportsProperMotionUnavailable()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.starAstrometry->properMotionDeclinationMasPerYear = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::properMotion(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.equatorial->rightAscensionHours > body.fixedEquatorial->rightAscensionHours);
    QVERIFY(std::abs(result.equatorial->declinationDeg - body.fixedEquatorial->declinationDeg) < 0.00001);
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::properMotion()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::properMotion()
    ));
}

void StarAstrometryCalculatorTests::degradesFixedOnlyStarsWhenAstrometryCorrectionsAreRequested()
{
    OwnGalaxyCelestialBody body = makeAstrometricStar();
    body.starAstrometry = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::properMotion(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(result.equatorial.has_value());
    compareCoordinates(*result.equatorial, *body.fixedEquatorial, 0.0000001);
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
}

void StarAstrometryCalculatorTests::failsWhenNoCoordinateFallbackExists()
{
    OwnGalaxyCelestialBody body;
    body.id = "missing-coordinate";
    body.kind = BaseCelestialBody::Kind::Star;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::properMotion(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(!result.equatorial.has_value());
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
}

QTEST_APPLESS_MAIN(StarAstrometryCalculatorTests)

#include "StarAstrometryCalculatorTests.moc"
