#include "OwnGalaxyCelestialBody.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <limits>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] OwnGalaxyCelestialBody makeBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] EphemerisRequest makeRequest()
{
    EphemerisRequest request;
    request.context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::atmosphericRefraction());
    request.options.setEnableAtmosphericRefraction(true);
    request.options.setAtmosphericPressureHpa(1010.0);
    request.options.setAtmosphericTemperatureC(10.0);
    request.options.setRelativeHumidity(0.5);
    request.options.setObservingWavelengthMicrometers(0.55);
    return request;
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const EphemerisRequest& request)
{
    static const OwnGalaxyCelestialBody kBody = makeBody();
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
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.dataSourceProvenance = "unit-test apparent place";
    return result;
}

void verifyRefractionUnavailable(const HighPrecisionCalculatorResult& result, const double expectedAltitudeDeg)
{
    QVERIFY(result.horizontal.has_value());
    QCOMPARE(result.horizontal->altitudeDeg, expectedAltitudeDeg);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
    ));
}

}  // namespace

class AtmosphericRefractionCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void appliesRefractionWhenRequested();
    void leavesResultUnchangedWhenDisabled();
    void reportsMissingHorizontalCoordinates();
    void reportsInvalidObserverInput();
    void reportsInvalidAtmosphereInputs_data();
    void reportsInvalidAtmosphereInputs();
    void skipsRefractionBelowModelAltitude();
    void appliesRefractionAtModelAltitudeBoundaries();
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
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    QVERIFY(!result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
}

void AtmosphericRefractionCalculatorTests::leavesResultUnchangedWhenDisabled()
{
    const AtmosphericRefractionCalculator calculator;
    EphemerisRequest request = makeRequest();
    request.options.setEnableAtmosphericRefraction(false);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(20.0));

    QVERIFY(result.horizontal.has_value());
    QCOMPARE(result.horizontal->altitudeDeg, 20.0);
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
    ));
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
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
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
}

void AtmosphericRefractionCalculatorTests::reportsInvalidObserverInput()
{
    const AtmosphericRefractionCalculator calculator;
    EphemerisRequest request = makeRequest();
    request.context.observer.latitudeDeg = core::GeoLocation::kLatitudeMaxDeg + 1.0;

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(20.0));

    verifyRefractionUnavailable(result, 20.0);
}

void AtmosphericRefractionCalculatorTests::reportsInvalidAtmosphereInputs_data()
{
    QTest::addColumn<double>("pressureHpa");
    QTest::addColumn<double>("temperatureC");
    QTest::addColumn<double>("relativeHumidity");
    QTest::addColumn<double>("wavelengthMicrometers");

    const EphemerisRequest validRequest = makeRequest();
    const double validPressureHpa = validRequest.options.atmosphericPressureHpa();
    const double validTemperatureC = validRequest.options.atmosphericTemperatureC();
    const double validRelativeHumidity = validRequest.options.relativeHumidity();
    const double validWavelengthMicrometers = validRequest.options.observingWavelengthMicrometers();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QTest::newRow("negative pressure") << -1.0 << validTemperatureC << validRelativeHumidity
                                       << validWavelengthMicrometers;
    QTest::newRow("zero pressure") << 0.0 << validTemperatureC << validRelativeHumidity << validWavelengthMicrometers;
    QTest::newRow("high pressure") << 1200.1 << validTemperatureC << validRelativeHumidity
                                   << validWavelengthMicrometers;
    QTest::newRow("non-finite pressure") << nan << validTemperatureC << validRelativeHumidity
                                         << validWavelengthMicrometers;
    QTest::newRow("low temperature") << validPressureHpa << -100.1 << validRelativeHumidity
                                     << validWavelengthMicrometers;
    QTest::newRow("high temperature") << validPressureHpa << 80.1 << validRelativeHumidity
                                      << validWavelengthMicrometers;
    QTest::newRow("non-finite temperature")
        << validPressureHpa << nan << validRelativeHumidity << validWavelengthMicrometers;
    QTest::newRow("low relative humidity")
        << validPressureHpa << validTemperatureC << -0.1 << validWavelengthMicrometers;
    QTest::newRow("high relative humidity")
        << validPressureHpa << validTemperatureC << 1.1 << validWavelengthMicrometers;
    QTest::newRow("non-finite relative humidity")
        << validPressureHpa << validTemperatureC << nan << validWavelengthMicrometers;
    QTest::newRow("low wavelength") << validPressureHpa << validTemperatureC << validRelativeHumidity << 0.09;
    QTest::newRow("high wavelength") << validPressureHpa << validTemperatureC << validRelativeHumidity << 100.1;
    QTest::newRow("non-finite wavelength") << validPressureHpa << validTemperatureC << validRelativeHumidity << nan;
}

void AtmosphericRefractionCalculatorTests::reportsInvalidAtmosphereInputs()
{
    const AtmosphericRefractionCalculator calculator;
    EphemerisRequest request = makeRequest();
    QFETCH(double, pressureHpa);
    QFETCH(double, temperatureC);
    QFETCH(double, relativeHumidity);
    QFETCH(double, wavelengthMicrometers);

    request.options.setAtmosphericPressureHpa(pressureHpa);
    request.options.setAtmosphericTemperatureC(temperatureC);
    request.options.setRelativeHumidity(relativeHumidity);
    request.options.setObservingWavelengthMicrometers(wavelengthMicrometers);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(20.0));

    verifyRefractionUnavailable(result, 20.0);
}

void AtmosphericRefractionCalculatorTests::skipsRefractionBelowModelAltitude()
{
    const AtmosphericRefractionCalculator calculator;
    const EphemerisRequest request = makeRequest();

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult(-2.0));

    QVERIFY(result.horizontal.has_value());
    QCOMPARE(result.horizontal->altitudeDeg, -2.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(!result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.unavailableCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
    ));
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
    ));
}

void AtmosphericRefractionCalculatorTests::appliesRefractionAtModelAltitudeBoundaries()
{
    const AtmosphericRefractionCalculator calculator;
    const EphemerisRequest request = makeRequest();

    const HighPrecisionCalculatorResult minimumResult =
        calculator.apply(makeInput(request), makeCalculatorResult(-1.0));
    QVERIFY(minimumResult.horizontal.has_value());
    QVERIFY(minimumResult.horizontal->altitudeDeg > -1.0);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            minimumResult.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    QVERIFY(!minimumResult.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));

    const HighPrecisionCalculatorResult clampedHighResult =
        calculator.apply(makeInput(request), makeCalculatorResult(89.95));
    QVERIFY(clampedHighResult.horizontal.has_value());
    QVERIFY(clampedHighResult.horizontal->altitudeDeg <= 90.0);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            clampedHighResult.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
    QVERIFY(!clampedHighResult.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));

    const HighPrecisionCalculatorResult zenithResult = calculator.apply(makeInput(request), makeCalculatorResult(90.0));
    verifyRefractionUnavailable(zenithResult, 90.0);
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
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
}

QTEST_APPLESS_MAIN(AtmosphericRefractionCalculatorTests)

#include "AtmosphericRefractionCalculatorTests.moc"
