#include "math/MathConstants.hpp"
#include "engine/highprecision/ICalcephKernelProvider.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

using core::MathConstants;

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

[[nodiscard]] CelestialBody makePartialAstrometricStar()
{
    CelestialBody body = makeAstrometricStar();
    body.id = "partial-star";
    body.displayName = "Partial Star";
    body.starAstrometry->properMotionDeclinationMasPerYear = std::nullopt;
    body.starAstrometry->stellarParallaxMas = std::nullopt;
    body.starAstrometry->radialVelocityKmPerSecond = std::nullopt;
    return body;
}

[[nodiscard]] CelestialBody makeFixedOnlyStar()
{
    CelestialBody body;
    body.id = "fixed-star";
    body.displayName = "Fixed Star";
    body.type = CelestialBodyType::Star;
    body.ephemerisSource = CelestialBodyEphemerisSource::FixedEquatorial;
    body.fixedEquatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = -15.0,
    };
    return body;
}

[[nodiscard]] CelestialBody makeStarWithInvalidOptionalAstrometry()
{
    CelestialBody body = makeAstrometricStar();
    body.id = "invalid-optional-star";
    body.displayName = "Invalid Optional Star";
    body.starAstrometry->properMotionRightAscensionMasPerYear = std::numeric_limits<double>::quiet_NaN();
    body.starAstrometry->properMotionDeclinationMasPerYear = std::numeric_limits<double>::infinity();
    body.starAstrometry->stellarParallaxMas = std::numeric_limits<double>::quiet_NaN();
    body.starAstrometry->radialVelocityKmPerSecond = -std::numeric_limits<double>::infinity();
    return body;
}

[[nodiscard]] CelestialBody makePlanet()
{
    CelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.type = CelestialBodyType::Planet;
    body.ephemerisSource = CelestialBodyEphemerisSource::Planet;
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

[[nodiscard]] double
angularSeparationDegrees(const core::EquatorialCoordinate& lhs, const core::EquatorialCoordinate& rhs) noexcept
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
        QCOMPARE(actual.observerRelativePositionAu->xAu, expected.observerRelativePositionAu->xAu);
        QCOMPARE(actual.observerRelativePositionAu->yAu, expected.observerRelativePositionAu->yAu);
        QCOMPARE(actual.observerRelativePositionAu->zAu, expected.observerRelativePositionAu->zAu);
    }
    QCOMPARE(actual.metadata.status, expected.metadata.status);
    QCOMPARE(actual.metadata.appliedCorrections, expected.metadata.appliedCorrections);
    QCOMPARE(actual.metadata.unavailableCorrections, expected.metadata.unavailableCorrections);
    QCOMPARE(actual.metadata.warningCodeMask, expected.metadata.warningCodeMask);
}

class FixedEarthKernelProvider final : public ICalcephKernelProvider {
public:
    explicit FixedEarthKernelProvider(
        std::optional<SolarSystemKernelVector> earthPositionAu, const bool requireTdbEpoch = false
    )
        : m_earthPositionAu(earthPositionAu), m_requireTdbEpoch(requireTdbEpoch)
    {
    }

    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        ++m_callCount;
        m_lastEpoch = epoch;
        m_lastTargetNaifId = targetNaifId;
        m_lastCenterNaifId = centerNaifId;

        SolarSystemKernelStateResult result;
        if (m_requireTdbEpoch && epoch.timeScale != TimeScale::Tdb) {
            result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            return result;
        }

        result.positionAu = m_earthPositionAu;
        result.metadata.status = m_earthPositionAu.has_value()
                                     ? skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid
                                     : skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.dataSourceProvenance = "unit-test Earth barycentric state";
        if (!m_earthPositionAu.has_value()) {
            result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
        }
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] int lastTargetNaifId() const noexcept
    {
        return m_lastTargetNaifId;
    }

    [[nodiscard]] int lastCenterNaifId() const noexcept
    {
        return m_lastCenterNaifId;
    }

    [[nodiscard]] AstronomicalEpoch lastEpoch() const noexcept
    {
        return m_lastEpoch;
    }

private:
    std::optional<SolarSystemKernelVector> m_earthPositionAu;
    bool m_requireTdbEpoch = false;
    mutable int m_callCount = 0;
    mutable int m_lastTargetNaifId = 0;
    mutable int m_lastCenterNaifId = 0;
    mutable AstronomicalEpoch m_lastEpoch;
};

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
    const CelestialBody body = makeAstrometricStar();
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
        core::EquatorialCoordinate{
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
    const CelestialBody body = makeAstrometricStar();
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
    CelestialBody body = makeAstrometricStar();
    body.fixedEquatorial = core::EquatorialCoordinate{
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
        core::EquatorialCoordinate{
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
    const CelestialBody body = makeAstrometricStar();
    const EphemerisRequest referenceRequest = makeRequest(EphemerisCorrectionFlags::stellarParallax(), 0.0);
    const EphemerisRequest parallaxRequest = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);
    auto kernelProvider =
        std::make_shared<FixedEarthKernelProvider>(SolarSystemKernelVector{.xAu = 0.0, .yAu = 1.0, .zAu = 0.0}, true);
    auto timeScaleService = std::make_shared<FixedTdbTimeScaleService>();

    const StarAstrometryCalculator calculator(kernelProvider, timeScaleService);
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
    QCOMPARE(kernelProvider->callCount(), 1);
    QCOMPARE(kernelProvider->lastTargetNaifId(), 399);
    QCOMPARE(kernelProvider->lastCenterNaifId(), 0);
    QCOMPARE(
        static_cast<std::uint8_t>(kernelProvider->lastEpoch().timeScale), static_cast<std::uint8_t>(TimeScale::Tdb)
    );
    QCOMPARE(timeScaleService->callCount(), 1);
    QCOMPARE(static_cast<std::uint8_t>(timeScaleService->lastTargetScale()), static_cast<std::uint8_t>(TimeScale::Tdb));
}

void StarAstrometryCalculatorTests::batchMatchesSingleStarPropagationForFullPartialAndFixedStars()
{
    const std::vector<CelestialBody> bodies{
        makePlanet(),
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };
    const CatalogStarAstrometryArrays arrays(bodies);
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
    const std::vector<CelestialBody> bodies{
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };
    const CatalogStarAstrometryArrays arrays(bodies);
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
    const std::vector<CelestialBody> bodies{
        makeStarWithInvalidOptionalAstrometry(),
    };
    const CatalogStarAstrometryArrays arrays(bodies);
    const EphemerisRequest request = makeRequest(
        EphemerisCorrectionFlags::properMotion() | EphemerisCorrectionFlags::stellarParallax()
            | EphemerisCorrectionFlags::radialVelocity(),
        10.0
    );

    const StarAstrometryCalculator calculator;
    const std::vector<StarAstrometryBatchResult> batchResults = calculator.calculateBatch(request, arrays);
    const HighPrecisionCalculatorResult singleResult = calculator.calculate(makeInput(bodies[0], request));

    QCOMPARE(batchResults.size(), 1U);
    QCOMPARE(batchResults[0].bodyIndex, 0U);
    compareCalculatorResults(batchResults[0].result, singleResult);
    QCOMPARE(singleResult.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded);
    QVERIFY(singleResult.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
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
    const std::vector<CelestialBody> bodies{
        makeAstrometricStar(),
        makeAstrometricStar(),
        makePartialAstrometricStar(),
        makeFixedOnlyStar(),
    };
    const CatalogStarAstrometryArrays arrays(bodies);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);
    auto kernelProvider =
        std::make_shared<FixedEarthKernelProvider>(SolarSystemKernelVector{.xAu = 0.0, .yAu = 1.0, .zAu = 0.0}, true);
    auto timeScaleService = std::make_shared<FixedTdbTimeScaleService>();

    const StarAstrometryCalculator calculator(kernelProvider, timeScaleService);
    const std::vector<StarAstrometryBatchResult> batchResults = calculator.calculateBatch(request, arrays);

    QCOMPARE(batchResults.size(), 4U);
    QCOMPARE(kernelProvider->callCount(), 1);
    QCOMPARE(timeScaleService->callCount(), 1);
    for (const StarAstrometryBatchResult& batchResult : batchResults) {
        const HighPrecisionCalculatorResult singleResult =
            calculator.calculate(makeInput(bodies[batchResult.bodyIndex], request));
        compareCalculatorResults(batchResult.result, singleResult);
    }
    QCOMPARE(kernelProvider->lastTargetNaifId(), 399);
    QCOMPARE(kernelProvider->lastCenterNaifId(), 0);
}

void StarAstrometryCalculatorTests::degradesAnnualParallaxWhenKernelProviderIsMissing()
{
    const CelestialBody body = makeAstrometricStar();
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
    CelestialBody body = makeAstrometricStar();
    body.starAstrometry->stellarParallaxMas = std::nullopt;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::annualParallax(), 0.0);
    auto kernelProvider =
        std::make_shared<FixedEarthKernelProvider>(SolarSystemKernelVector{.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});

    const StarAstrometryCalculator calculator(kernelProvider);
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
    QCOMPARE(kernelProvider->callCount(), 0);
}

void StarAstrometryCalculatorTests::degradesRadialVelocityWhenStellarParallaxIsDisabled()
{
    CelestialBody body = makeAstrometricStar();
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
    CelestialBody body = makeAstrometricStar();
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
    CelestialBody body = makeAstrometricStar();
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
    CelestialBody body;
    body.id = "missing-coordinate";
    body.type = CelestialBodyType::Star;
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::properMotion(), 10.0);

    const StarAstrometryCalculator calculator;
    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(body, request));

    QVERIFY(!result.equatorial.has_value());
    QCOMPARE(result.metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed);
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
}

QTEST_APPLESS_MAIN(StarAstrometryCalculatorTests)

#include "StarAstrometryCalculatorTests.moc"
