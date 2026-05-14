#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"

#include <QtTest/QtTest>

#include <cstdint>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] CelestialBody makeBody()
{
    return {
        .id = "mars",
        .displayName = "Mars",
        .type = CelestialBodyType::Planet,
        .ephemerisSource = CelestialBodyEphemerisSource::Planet,
    };
}

[[nodiscard]] EphemerisRequest makeRequest()
{
    EphemerisRequest request;
    request.context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = EphemerisCorrectionFlags::AtmosphericRefraction;
    request.options.enableAtmosphericRefraction = true;
    request.options.atmosphericPressureHpa = 1010.0;
    request.options.atmosphericTemperatureC = 10.0;
    request.options.relativeHumidity = 0.5;
    request.options.observingWavelengthMicrometers = 0.55;
    return request;
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const EphemerisRequest& request)
{
    static const CelestialBody kBody = makeBody();
    return {
        .request = request,
        .body = kBody,
        .bodyIndex = 0U,
    };
}

[[nodiscard]] HighPrecisionCalculatorResult makeCalculatorResult(const double altitudeDeg)
{
    HighPrecisionCalculatorResult result;
    result.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 20.0,
    };
    result.horizontal = core::HorizontalCoordinate{
        .altitudeDeg = altitudeDeg,
        .azimuthDeg = 180.0,
    };
    result.metadata.status = EphemerisResultStatus::Valid;
    result.metadata.dataSourceProvenance = "unit-test apparent place";
    return result;
}

}  // namespace

class AtmosphericRefractionCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void appliesRefractionWhenRequested();
    void leavesResultUnchangedWhenDisabled();
    void reportsMissingHorizontalCoordinates();
    void reportsInvalidAtmosphereInputs();
    void reportsAltitudeOutsideModelRange();
    void appliesSmallCorrectionNearZenith();
};

void AtmosphericRefractionCalculatorTests::appliesRefractionWhenRequested()
{
    const AtmosphericRefractionCalculator calculator;
    const EphemerisRequest request = makeRequest();
    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(45.0));

    QVERIFY(result.horizontal.has_value());
    QVERIFY(result.horizontal->altitudeDeg > 45.0);
    QVERIFY(result.horizontal->altitudeDeg < 45.03);
    QCOMPARE(result.horizontal->azimuthDeg, 180.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction));
    QVERIFY(!result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

void AtmosphericRefractionCalculatorTests::leavesResultUnchangedWhenDisabled()
{
    const AtmosphericRefractionCalculator calculator;
    EphemerisRequest request = makeRequest();
    request.options.enableAtmosphericRefraction = false;

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(20.0));

    QVERIFY(result.horizontal.has_value());
    QCOMPARE(result.horizontal->altitudeDeg, 20.0);
    QVERIFY(!hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction));
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void AtmosphericRefractionCalculatorTests::reportsMissingHorizontalCoordinates()
{
    const AtmosphericRefractionCalculator calculator;
    const EphemerisRequest request = makeRequest();
    HighPrecisionCalculatorResult input = makeCalculatorResult(20.0);
    input.horizontal.reset();

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), input);

    QVERIFY(!result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

void AtmosphericRefractionCalculatorTests::reportsInvalidAtmosphereInputs()
{
    const AtmosphericRefractionCalculator calculator;
    EphemerisRequest request = makeRequest();
    request.options.atmosphericPressureHpa = -1.0;

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(20.0));

    QVERIFY(result.horizontal.has_value());
    QCOMPARE(result.horizontal->altitudeDeg, 20.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
    QVERIFY(!hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction));
}

void AtmosphericRefractionCalculatorTests::reportsAltitudeOutsideModelRange()
{
    const AtmosphericRefractionCalculator calculator;
    const EphemerisRequest request = makeRequest();

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(-2.0));

    QVERIFY(result.horizontal.has_value());
    QCOMPARE(result.horizontal->altitudeDeg, -2.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

void AtmosphericRefractionCalculatorTests::appliesSmallCorrectionNearZenith()
{
    const AtmosphericRefractionCalculator calculator;
    const EphemerisRequest request = makeRequest();

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(89.0));

    QVERIFY(result.horizontal.has_value());
    QVERIFY(result.horizontal->altitudeDeg > 89.0);
    QVERIFY(result.horizontal->altitudeDeg <= 90.0);
    QVERIFY(result.horizontal->altitudeDeg < 89.001);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction));
}

QTEST_APPLESS_MAIN(AtmosphericRefractionCalculatorTests)

#include "AtmosphericRefractionCalculatorTests.moc"
