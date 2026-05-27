#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"
#include "engine/highprecision/ICalcephKernelProvider.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include <QFile>
#include <QStringList>
#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

using skygate::core::MathConstants;
using skygate::core::PhysicalConstants;

[[nodiscard]] EphemerisRequest makeRequest()
{
    EphemerisRequest request;
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::geometric());
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
    struct Call {
        AstronomicalEpoch epoch;
        int targetNaifId = 0;
        int centerNaifId = 0;
    };

    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        ++callCount;
        lastEpoch = epoch;
        lastTargetNaifId = targetNaifId;
        lastCenterNaifId = centerNaifId;
        calls.push_back({
            .epoch = epoch,
            .targetNaifId = targetNaifId,
            .centerNaifId = centerNaifId,
        });
        const std::pair key{targetNaifId, centerNaifId};
        if (const auto sequence = responseSequences.find(key); sequence != responseSequences.end()) {
            const std::size_t index = responseSequenceIndexes[key]++;
            if (index < sequence->second.size()) {
                return sequence->second[index];
            }
            return sequence->second.back();
        }
        if (const auto match = responses.find(key); match != responses.end()) {
            return match->second;
        }
        return nextResult;
    }

    mutable int callCount = 0;
    mutable AstronomicalEpoch lastEpoch;
    mutable int lastTargetNaifId = 0;
    mutable int lastCenterNaifId = 0;
    mutable std::vector<Call> calls;
    mutable std::map<std::pair<int, int>, std::size_t> responseSequenceIndexes;
    std::map<std::pair<int, int>, SolarSystemKernelStateResult> responses;
    std::map<std::pair<int, int>, std::vector<SolarSystemKernelStateResult>> responseSequences;
    SolarSystemKernelStateResult nextResult;
};

[[nodiscard]] SolarSystemKernelStateResult makeKernelVector(
    const SolarSystemKernelVector& vector, const std::optional<SolarSystemKernelVector>& velocity = std::nullopt
)
{
    SolarSystemKernelStateResult result;
    result.positionAu = vector;
    result.velocityAuPerDay = velocity;
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
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

struct LightTimeTargetState {
    AstronomicalEpoch epoch;
    SolarSystemKernelVector vector;
};

struct LightTimeFixture {
    int targetNaifId = 0;
    int earthNaifId = 0;
    int barycenterNaifId = 0;
    AstronomicalEpoch receiveEpoch;
    SolarSystemKernelVector geometricVector;
    SolarSystemKernelVector earthReceiveVector;
    std::vector<LightTimeTargetState> retardedTargetStates;
    double expectedRightAscensionHours = 0.0;
    double expectedDeclinationDeg = 0.0;
    double tolerance = 0.0;
};

[[nodiscard]] GeometricFixture loadSmokeFixture()
{
    QFile file(QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/geometric_solar_system_smoke.csv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Unable to open geometric solar-system smoke fixture.");
    }

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

[[nodiscard]] LightTimeFixture loadLightTimeFixture()
{
    QFile file(QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/light_time_solar_system_mars.csv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Unable to open light-time solar-system smoke fixture.");
    }

    LightTimeFixture fixture;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields = line.split(QLatin1Char(','));
        Q_ASSERT(fields.size() == 11);
        const QString role = fields[1];
        const int targetNaifId = fields[2].toInt();
        const int centerNaifId = fields[3].toInt();
        const AstronomicalEpoch epoch{
            .julianDatePart1 = fields[4].toDouble(),
            .julianDatePart2 = 0.0,
            .timeScale = TimeScale::Tdb,
        };
        const SolarSystemKernelVector vector{
            .xAu = fields[5].toDouble(),
            .yAu = fields[6].toDouble(),
            .zAu = fields[7].toDouble(),
        };

        if (role == QStringLiteral("geometric")) {
            fixture.targetNaifId = targetNaifId;
            fixture.earthNaifId = centerNaifId;
            fixture.receiveEpoch = epoch;
            fixture.geometricVector = vector;
        } else if (role == QStringLiteral("earth-receive")) {
            fixture.earthNaifId = targetNaifId;
            fixture.barycenterNaifId = centerNaifId;
            fixture.earthReceiveVector = vector;
        } else if (role == QStringLiteral("target-retarded")) {
            fixture.retardedTargetStates.push_back({
                .epoch = epoch,
                .vector = vector,
            });
        } else if (role == QStringLiteral("expected-light-time")) {
            fixture.expectedRightAscensionHours = fields[8].toDouble();
            fixture.expectedDeclinationDeg = fields[9].toDouble();
            fixture.tolerance = fields[10].toDouble();
        } else {
            Q_UNREACHABLE();
        }
    }

    Q_ASSERT(fixture.targetNaifId != 0);
    Q_ASSERT(fixture.earthNaifId != 0);
    Q_ASSERT(fixture.barycenterNaifId == 0);
    Q_ASSERT(!fixture.retardedTargetStates.empty());
    Q_ASSERT(fixture.tolerance > 0.0);
    return fixture;
}

[[nodiscard]] double epochTotal(const AstronomicalEpoch& epoch) noexcept
{
    return epoch.julianDatePart1 + epoch.julianDatePart2;
}

}  // namespace

class SolarSystemStateCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void computesGeometricRaDecFromKernelVector();
    void computesGeometricRaDecAgainstHorizonsSmokeFixture();
    void mapsSupportedBodiesToNaifIds();
    void fallsBackToPlanetarySystemBarycenterWhenBodyCenterIsMissing();
    void prefersPlanetarySystemBarycenterWhenConfigured();
    void usesAvailableBodyCentersWhenBarycenterPreferenceConfigured();
    void appliesLightTimeCorrectionFromRetardedTargetAndReceiveEarth();
    void computesLightTimeRaDecAgainstHorizonsFixture();
    void reportsUnavailableLightTimeInputsWithoutDroppingGeometricResult();
    void appliesStellarAberrationFromEarthVelocity();
    void skipsStellarAberrationWhenDisabled();
    void reportsUnavailableStellarAberrationInputsWithoutDroppingGeometricResult();
    void appliesSolarGravitationalLightDeflection();
    void skipsSolarGravitationalLightDeflectionWhenDisabled();
    void reportsUnavailableSolarDeflectionInputsWithoutDroppingGeometricResult();
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
    QVERIFY(result.observerRelativePositionAu.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(std::abs(result.equatorial->declinationDeg - 45.0) < 1.0e-12);
    QCOMPARE(result.observerRelativePositionAu->xAu, 0.0);
    QCOMPARE(result.observerRelativePositionAu->yAu, 1.0);
    QCOMPARE(result.observerRelativePositionAu->zAu, 1.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::geometric())
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

void SolarSystemStateCalculatorTests::fallsBackToPlanetarySystemBarycenterWhenBodyCenterIsMissing()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    SolarSystemKernelStateResult missingBodyCenter;
    missingBodyCenter.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
    missingBodyCenter.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    provider->responses[{499, 399}] = missingBodyCenter;
    provider->responses[{4, 399}] = makeKernelVector({.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QCOMPARE(provider->callCount, 2);
    QCOMPARE(provider->calls[0].targetNaifId, 499);
    QCOMPARE(provider->calls[1].targetNaifId, 4);
    QCOMPARE(provider->lastCenterNaifId, 399);
    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::BarycenterFallback));
    QVERIFY(result.metadata.dataSourceProvenance.find("mars body center (499)") != std::string::npos);
    QVERIFY(result.metadata.dataSourceProvenance.find("planetary-system barycenter (4)") != std::string::npos);
}

void SolarSystemStateCalculatorTests::prefersPlanetarySystemBarycenterWhenConfigured()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    provider->responses[{4, 399}] = makeKernelVector({.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider, true);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QCOMPARE(provider->callCount, 1);
    QCOMPARE(provider->lastTargetNaifId, 4);
    QCOMPARE(provider->lastCenterNaifId, 399);
    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(!result.metadata.hasWarning(EphemerisEngineWarning::Code::BarycenterFallback));
    QVERIFY(result.metadata.dataSourceProvenance == std::string{"Horizons ICRF geometric fixture"});
}

void SolarSystemStateCalculatorTests::usesAvailableBodyCentersWhenBarycenterPreferenceConfigured()
{
    struct Case {
        std::string_view id;
        int bodyCenterNaifId = 0;
        int barycenterNaifId = 0;
    };
    const std::array cases{
        Case{.id = "mercury", .bodyCenterNaifId = 199, .barycenterNaifId = 1},
        Case{.id = "venus", .bodyCenterNaifId = 299, .barycenterNaifId = 2},
    };

    for (const Case& item : cases) {
        const auto provider = std::make_shared<FakeCalcephKernelProvider>();
        provider->responses[{item.bodyCenterNaifId, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
        provider->responses[{item.barycenterNaifId, 399}] = makeKernelVector({.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});
        const SolarSystemStateCalculator calculator(provider, true);

        const HighPrecisionCalculatorResult result =
            calculator.calculate(makeInput(makePlanetBody(std::string{item.id}), makeRequest()));

        QCOMPARE(provider->callCount, 1);
        QCOMPARE(provider->lastTargetNaifId, item.bodyCenterNaifId);
        QCOMPARE(provider->lastCenterNaifId, 399);
        QVERIFY(result.equatorial.has_value());
        QCOMPARE(
            static_cast<std::uint8_t>(result.metadata.status),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
        );
        QVERIFY(!result.metadata.hasWarning(EphemerisEngineWarning::Code::BarycenterFallback));
    }
}

void SolarSystemStateCalculatorTests::appliesLightTimeCorrectionFromRetardedTargetAndReceiveEarth()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    provider->responses[{399, 0}] = makeKernelVector({.xAu = 10.0, .yAu = 0.0, .zAu = 0.0});
    provider->responses[{499, 0}] = makeKernelVector({.xAu = 10.0, .yAu = 1.0, .zAu = 1.0});
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::lightTime());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QVERIFY(provider->callCount >= 3);
    QCOMPARE(provider->calls.front().targetNaifId, 499);
    QCOMPARE(provider->calls.front().centerNaifId, 399);
    QCOMPARE(provider->calls[1].targetNaifId, 399);
    QCOMPARE(provider->calls[1].centerNaifId, 0);
    QCOMPARE(provider->lastTargetNaifId, 499);
    QCOMPARE(provider->lastCenterNaifId, 0);
    QVERIFY(
        provider->lastEpoch.julianDatePart1 + provider->lastEpoch.julianDatePart2
        < request.epoch.julianDatePart1 + request.epoch.julianDatePart2
    );
    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.observerRelativePositionAu.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(std::abs(result.equatorial->declinationDeg - 45.0) < 1.0e-12);
    QCOMPARE(result.observerRelativePositionAu->xAu, 0.0);
    QCOMPARE(result.observerRelativePositionAu->yAu, 1.0);
    QCOMPARE(result.observerRelativePositionAu->zAu, 1.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
}

void SolarSystemStateCalculatorTests::computesLightTimeRaDecAgainstHorizonsFixture()
{
    const LightTimeFixture fixture = loadLightTimeFixture();
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{fixture.targetNaifId, fixture.earthNaifId}] = makeKernelVector(fixture.geometricVector);
    provider->responses[{fixture.earthNaifId, fixture.barycenterNaifId}] = makeKernelVector(fixture.earthReceiveVector);
    for (const LightTimeTargetState& targetState : fixture.retardedTargetStates) {
        provider->responseSequences[{fixture.targetNaifId, fixture.barycenterNaifId}].push_back(
            makeKernelVector(targetState.vector)
        );
    }
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.epoch = fixture.receiveEpoch;
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::lightTime());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QCOMPARE(provider->callCount, 2 + static_cast<int>(fixture.retardedTargetStates.size()));
    QCOMPARE(provider->calls[0].targetNaifId, fixture.targetNaifId);
    QCOMPARE(provider->calls[0].centerNaifId, fixture.earthNaifId);
    QCOMPARE(provider->calls[1].targetNaifId, fixture.earthNaifId);
    QCOMPARE(provider->calls[1].centerNaifId, fixture.barycenterNaifId);
    for (std::size_t index = 0; index < fixture.retardedTargetStates.size(); ++index) {
        const FakeCalcephKernelProvider::Call& call = provider->calls[index + 2U];
        QCOMPARE(call.targetNaifId, fixture.targetNaifId);
        QCOMPARE(call.centerNaifId, fixture.barycenterNaifId);
        QVERIFY(std::abs(epochTotal(call.epoch) - epochTotal(fixture.retardedTargetStates[index].epoch)) < 1.0e-9);
    }
    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - fixture.expectedRightAscensionHours) < fixture.tolerance);
    QVERIFY(std::abs(result.equatorial->declinationDeg - fixture.expectedDeclinationDeg) < fixture.tolerance);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
}

void SolarSystemStateCalculatorTests::reportsUnavailableLightTimeInputsWithoutDroppingGeometricResult()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});
    provider->responses[{399, 0}].metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
    provider->responses[{399, 0}].metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::lightTime());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QCOMPARE(provider->callCount, 2);
    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(std::abs(result.equatorial->declinationDeg) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
    ));
}

void SolarSystemStateCalculatorTests::appliesStellarAberrationFromEarthVelocity()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    provider->responses[{399, 0}] = makeKernelVector(
        {.xAu = 0.0, .yAu = 0.0, .zAu = 0.0},
        SolarSystemKernelVector{.xAu = 0.0, .yAu = PhysicalConstants::kSpeedOfLightAuPerDay * 1.0e-4, .zAu = 0.0}
    );
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::stellarAberration());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.equatorial->rightAscensionHours > 0.0);
    QVERIFY(result.equatorial->rightAscensionHours < 1.0e-3);
    QVERIFY(std::abs(result.equatorial->declinationDeg) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::stellarAberration()
        )
    );
}

void SolarSystemStateCalculatorTests::skipsStellarAberrationWhenDisabled()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    provider->responses[{399, 0}] = makeKernelVector(
        {.xAu = 0.0, .yAu = 0.0, .zAu = 0.0},
        SolarSystemKernelVector{.xAu = 0.0, .yAu = PhysicalConstants::kSpeedOfLightAuPerDay * 1.0e-4, .zAu = 0.0}
    );
    const SolarSystemStateCalculator calculator(provider);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::stellarAberration()
    ));
}

void SolarSystemStateCalculatorTests::reportsUnavailableStellarAberrationInputsWithoutDroppingGeometricResult()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    provider->responses[{399, 0}] = makeKernelVector({.xAu = 0.0, .yAu = 0.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::stellarAberration());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::stellarAberration()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::stellarAberration()
    ));
}

void SolarSystemStateCalculatorTests::appliesSolarGravitationalLightDeflection()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = std::cos(0.1), .yAu = std::sin(0.1), .zAu = 0.0});
    provider->responses[{10, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::gravitationalLightDeflection());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.equatorial->rightAscensionHours > (0.1 * 12.0 / MathConstants::kPi));
    QVERIFY(std::abs(result.equatorial->declinationDeg) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.appliedCorrections, EphemerisCorrectionFlags::gravitationalLightDeflection()
        )
    );
}

void SolarSystemStateCalculatorTests::skipsSolarGravitationalLightDeflectionWhenDisabled()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = std::cos(0.1), .yAu = std::sin(0.1), .zAu = 0.0});
    provider->responses[{10, 399}] = makeKernelVector({.xAu = 1.0, .yAu = 0.0, .zAu = 0.0});
    const SolarSystemStateCalculator calculator(provider);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - (0.1 * 12.0 / MathConstants::kPi)) < 1.0e-12);
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::gravitationalLightDeflection()
    ));
}

void SolarSystemStateCalculatorTests::reportsUnavailableSolarDeflectionInputsWithoutDroppingGeometricResult()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->responses[{499, 399}] = makeKernelVector({.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});
    provider->responses[{10, 399}].metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
    provider->responses[{10, 399}].metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    const SolarSystemStateCalculator calculator(provider);
    EphemerisRequest request = makeRequest();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::gravitationalLightDeflection());

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), request));

    QVERIFY(result.equatorial.has_value());
    QVERIFY(std::abs(result.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(std::abs(result.equatorial->declinationDeg) < 1.0e-12);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            result.metadata.unavailableCorrections, EphemerisCorrectionFlags::gravitationalLightDeflection()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        result.metadata.appliedCorrections, EphemerisCorrectionFlags::gravitationalLightDeflection()
    ));
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
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported)
    );
    QVERIFY(unknownPlanet.metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedBody));
    QCOMPARE(
        static_cast<std::uint8_t>(deepSky.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported)
    );
}

void SolarSystemStateCalculatorTests::reportsMissingKernelProvider()
{
    const SolarSystemStateCalculator calculator(nullptr);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(!result.equatorial.has_value());
}

void SolarSystemStateCalculatorTests::propagatesOutOfRangeKernelStatus()
{
    const auto provider = std::make_shared<FakeCalcephKernelProvider>();
    provider->nextResult.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
    provider->nextResult.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    const SolarSystemStateCalculator calculator(provider);

    const HighPrecisionCalculatorResult result = calculator.calculate(makeInput(makePlanetBody("mars"), makeRequest()));

    QCOMPARE(provider->callCount, 1);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
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
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
}

QTEST_APPLESS_MAIN(SolarSystemStateCalculatorTests)

#include "SolarSystemStateCalculatorTests.moc"
