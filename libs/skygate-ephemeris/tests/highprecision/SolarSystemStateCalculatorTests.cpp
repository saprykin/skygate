#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include <QFile>
#include <QStringList>
#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

[[nodiscard]] EphemerisRequest makeRequest()
{
    EphemerisRequest request;
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = EphemerisCorrectionFlags::Geometric;
    return request;
}

[[nodiscard]] CelestialBody makePlanetBody(std::string id)
{
    return {
        .id = std::move(id),
        .displayName = "Planet",
        .type = CelestialBodyType::Planet,
        .ephemerisSource = CelestialBodyEphemerisSource::Planet,
    };
}

[[nodiscard]] CelestialBody makeSunBody()
{
    return {
        .id = "sun",
        .displayName = "Sun",
        .type = CelestialBodyType::Sun,
        .ephemerisSource = CelestialBodyEphemerisSource::Sun,
    };
}

[[nodiscard]] CelestialBody makeDeepSkyBody()
{
    return {
        .id = "m31",
        .displayName = "M31",
        .type = CelestialBodyType::DeepSkyObject,
        .ephemerisSource = CelestialBodyEphemerisSource::Unresolved,
    };
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const CelestialBody& body, const EphemerisRequest& request)
{
    return {
        .request = request,
        .body = body,
        .bodyIndex = 4U,
    };
}

class FakeCalcephKernelProvider final : public ICalcephKernelProvider {
public:
    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        ++callCount;
        lastEpoch = epoch;
        lastTargetNaifId = targetNaifId;
        lastCenterNaifId = centerNaifId;
        return nextResult;
    }

    mutable int callCount = 0;
    mutable AstronomicalEpoch lastEpoch;
    mutable int lastTargetNaifId = 0;
    mutable int lastCenterNaifId = 0;
    SolarSystemKernelStateResult nextResult;
};

[[nodiscard]] SolarSystemKernelStateResult makeKernelVector(const SolarSystemKernelVector& vector)
{
    SolarSystemKernelStateResult result;
    result.positionAu = vector;
    result.metadata.status = EphemerisResultStatus::Valid;
    result.metadata.dataSourceProvenance = "Horizons ICRF geometric fixture";
    return result;
}

struct GeometricFixture {
    int targetNaifId = 0;
    int centerNaifId = 0;
    AstronomicalEpoch epoch;
    SolarSystemKernelVector vector;
    double expectedRightAscensionHours = 0.0;
    double expectedDeclinationDeg = 0.0;
    double toleranceDeg = 0.0;
};

[[nodiscard]] GeometricFixture loadSmokeFixture()
{
    QFile file(QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/geometric_solar_system_smoke.csv"));
    Q_ASSERT(file.open(QIODevice::ReadOnly | QIODevice::Text));

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields = line.split(QLatin1Char(','));
        Q_ASSERT(fields.size() == 10);
        return GeometricFixture{
            .targetNaifId = fields[1].toInt(),
            .centerNaifId = fields[2].toInt(),
            .epoch = {.julianDatePart1 = fields[3].toDouble(), .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
            .vector =
                {
                    .xAu = fields[4].toDouble(),
                    .yAu = fields[5].toDouble(),
                    .zAu = fields[6].toDouble(),
                },
            .expectedRightAscensionHours = fields[7].toDouble(),
            .expectedDeclinationDeg = fields[8].toDouble(),
            .toleranceDeg = fields[9].toDouble(),
        };
    }

    Q_UNREACHABLE();
}

}  // namespace

class SolarSystemStateCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void computesGeometricRaDecFromKernelVector();
    void computesGeometricRaDecAgainstHorizonsSmokeFixture();
    void mapsSupportedBodiesToNaifIds();
    void reportsUnsupportedPlanetIdsWithoutCallingKernel();
    void reportsMissingKernelProvider();
    void propagatesOutOfRangeKernelStatus();
    void rejectsNonTdbEpochs();
};

void SolarSystemStateCalculatorTests::computesGeometricRaDecFromKernelVector()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult = makeKernelVector({.xAu = 0.0, .yAu = 1.0, .zAu = 1.0});
    const SolarSystemStateCalculator calculator(provider);
    const CelestialBody mars = makePlanetBody("mars");
    const EphemerisRequest request = makeRequest();

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(mars, request));

    QCOMPARE(provider->callCount, 1);
    QCOMPARE(provider->lastTargetNaifId, 499);
    QCOMPARE(provider->lastCenterNaifId, 399);
    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(std::abs(result.equatorial->declinationDeg - 45.0) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::Geometric)
    );
    QVERIFY(result.metadata.dataSourceProvenance == std::string{"Horizons ICRF geometric fixture"});
}

void SolarSystemStateCalculatorTests::computesGeometricRaDecAgainstHorizonsSmokeFixture()
{
    const GeometricFixture fixture = loadSmokeFixture();
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult = makeKernelVector(fixture.vector);
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.epoch = fixture.epoch;

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QCOMPARE(provider->lastTargetNaifId, fixture.targetNaifId);
    QCOMPARE(provider->lastCenterNaifId, fixture.centerNaifId);
    QVERIFY(result.equatorial.has_value());
    QVERIFY(
        std::abs(result.equatorial->rightAscensionHours - fixture.expectedRightAscensionHours) < fixture.toleranceDeg
    );
    QVERIFY(std::abs(result.equatorial->declinationDeg - fixture.expectedDeclinationDeg) < fixture.toleranceDeg);
}

void SolarSystemStateCalculatorTests::mapsSupportedBodiesToNaifIds()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);
    const EphemerisRequest request = makeRequest();

    struct Case {
        CelestialBody body;
        int naifId = 0;
    };
    const std::array cases{
        Case{.body = makeSunBody(), .naifId = 10},
        Case{.body = makePlanetBody("mercury"), .naifId = 199},
        Case{.body = makePlanetBody("venus"), .naifId = 299},
        Case{.body = makePlanetBody("jupiter"), .naifId = 599},
        Case{.body = makePlanetBody("saturn"), .naifId = 699},
        Case{.body = makePlanetBody("uranus"), .naifId = 799},
        Case{.body = makePlanetBody("neptune"), .naifId = 899},
        Case{.body = makePlanetBody("pluto"), .naifId = 999},
    };

    for (const Case& item : cases) {
        const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(item.body, request));

        QVERIFY(result.equatorial.has_value());
        QCOMPARE(provider->lastTargetNaifId, item.naifId);
        QCOMPARE(provider->lastCenterNaifId, 399);
    }
}

void SolarSystemStateCalculatorTests::reportsUnsupportedPlanetIdsWithoutCallingKernel()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);

    const HighPrecisionCalculatorResult unknownPlanet =
        calculator.calculate(makeInput(makePlanetBody("planet_x"), makeRequest()));
    const HighPrecisionCalculatorResult deepSky = calculator.calculate(makeInput(makeDeepSkyBody(), makeRequest()));

    QCOMPARE(provider->callCount, 0);
    QCOMPARE(
        static_cast<std::uint8_t>(unknownPlanet.metadata.status),
        static_cast<std::uint8_t>(EphemerisResultStatus::Unsupported)
    );
    QVERIFY(unknownPlanet.metadata.hasWarning(EphemerisWarningCode::UnsupportedBody));
    QCOMPARE(
        static_cast<std::uint8_t>(deepSky.metadata.status),
        static_cast<std::uint8_t>(EphemerisResultStatus::Unsupported)
    );
}

void SolarSystemStateCalculatorTests::reportsMissingKernelProvider()
{
    const SolarSystemStateCalculator calculator(nullptr);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
    QVERIFY(!result.equatorial.has_value());
}

void SolarSystemStateCalculatorTests::propagatesOutOfRangeKernelStatus()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult.metadata.status = EphemerisResultStatus::OutOfRange;
    provider->nextResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    const SolarSystemStateCalculator calculator(provider);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QCOMPARE(provider->callCount, 1);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::OutOfRange)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(!result.equatorial.has_value());
}

void SolarSystemStateCalculatorTests::rejectsNonTdbEpochs()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.epoch.timeScale = TimeScale::Utc;

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QCOMPARE(provider->callCount, 0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
}

QTEST_APPLESS_MAIN(SolarSystemStateCalculatorTests)

#include "SolarSystemStateCalculatorTests.moc"
