#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "EphemerisRequest.hpp"
#include "EphemerisRequestFactory.hpp"
#include "EquatorialCoordinate.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "TestHelpers.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/IStarAstrometryCalculator.hpp"
#include "engine/highprecision/StarAstrometryBatchResult.hpp"
#include "engine/simple/SimpleEphemerisEngine.hpp"
#include "time/EpochCodec.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {

using namespace skygate::core;
using namespace skygate::ephemeris;

[[nodiscard]] UtcTimePoint utcTime(const std::int64_t seconds)
{
    return UtcTimePoint(std::chrono::seconds(seconds));
}

[[nodiscard]] ObservationContext makeContext(const UtcTimePoint time)
{
    ObservationContext context;
    context.utcTime = time;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.observer.elevationMeters = 10.0;
    return context;
}

[[nodiscard]] OwnGalaxyCelestialBody makeSunBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "sun";
    body.displayName = "Sun";
    body.kind = BaseCelestialBody::Kind::Sun;
    return body;
}

[[nodiscard]] OwnGalaxyCelestialBody makeFixedStarBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "vega";
    body.displayName = "Vega";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = 6.0,
        .declinationDeg = 20.0,
    };
    return body;
}

#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS
using namespace skygate::ephemeris::highprecision;

class RecordingStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        m_epochs.push_back(input.request.epoch);
        HighPrecisionCalculatorResult result;
        result.equatorial = EquatorialCoordinate{
            .rightAscensionHours = 6.0,
            .declinationDeg = 20.0,
        };
        result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        return result;
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest&,
        const CatalogStarAstrometryArrays&,
        std::shared_ptr<const PreparedEphemerisRequestState> = {}
    ) const override
    {
        return {};
    }

    [[nodiscard]] const std::vector<AstronomicalEpoch>& epochs() const noexcept
    {
        return m_epochs;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_epochs.empty();
    }

private:
    mutable std::vector<AstronomicalEpoch> m_epochs;
};

[[nodiscard]] EphemerisEngineOptions highPrecisionStarOnlyOptions()
{
    EphemerisEngineOptions options;
    options.setEngineKind(EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(EphemerisCorrectionFlags::noCorrections());
    options.setFallbackToSimpleEngine(false);
    return options;
}

[[nodiscard]] HighPrecisionEphemerisEngine makeHighPrecisionEngine(
    const CelestialBodyCatalog& catalog, std::shared_ptr<RecordingStarAstrometryCalculator> calculator
)
{
    HighPrecisionEphemerisEngine::Dependencies dependencies;
    dependencies.starAstrometryCalculator = std::move(calculator);
    dependencies.dataSetInfo.id = "time-contract-test";
    dependencies.dataSetInfo.displayName = "Time contract test";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";
    return HighPrecisionEphemerisEngine(catalog, highPrecisionStarOnlyOptions(), std::move(dependencies));
}
#endif

}  // namespace

class EphemerisRequestTimeContractTests final : public QObject {
    Q_OBJECT

private slots:
    void simpleEngineRejectsExplicitNonUtcEpoch();
    void simpleEngineRejectsNonFiniteExplicitEpoch();
    void simpleEngineUsesExplicitUtcEpochOverStaleContext();
    void simpleEngineConvenienceOverloadMatchesExplicitUtcRequest();
    void simpleEngineZeroEpochRemainsContextOnly();
#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS
    void highPrecisionEngineUsesExplicitUtcEpoch();
    void highPrecisionEngineUsesExplicitTdbEpoch();
    void highPrecisionEngineConvenienceOverloadBuildsUtcEpoch();
    void highPrecisionEngineRejectsNonExplicitEpoch();
#endif
};

void EphemerisRequestTimeContractTests::simpleEngineRejectsExplicitNonUtcEpoch()
{
    const std::array<TimeScale, 4> scales{
        TimeScale::Tai,
        TimeScale::Tt,
        TimeScale::Tdb,
        TimeScale::Ut1,
    };
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeSunBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    const SimpleEphemerisEngine engine(catalog, SimpleEphemerisEngine::defaultOptions());

    const UtcTimePoint epochUtcTime = utcTime(1'704'067'200);
    const UtcTimePoint staleUtcTime = utcTime(0);

    for (const TimeScale scale : scales) {
        EphemerisRequest request;
        request.context = makeContext(staleUtcTime);
        request.epoch = EpochCodec::epochFromUtcTime(epochUtcTime);
        request.epoch.timeScale = scale;
        request.options = engine.options();

        const EphemerisSnapshot snapshot = engine.compute(request);
        QCOMPARE(snapshot.states.size(), std::size_t{1});
        const CelestialBodyState& state = snapshot.states.front();
        QCOMPARE(
            static_cast<std::uint8_t>(state.metadata.status),
            static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
        );
        QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion));
        QVERIFY(std::isnan(state.equatorial.rightAscensionHours));
        QVERIFY(std::isnan(state.equatorial.declinationDeg));

        const std::optional<CelestialBodyState> byId = engine.computeBodyState(request, "sun");
        QVERIFY(byId.has_value());
        QCOMPARE(
            static_cast<std::uint8_t>(byId->metadata.status),
            static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
        );
        QVERIFY(byId->metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion));
        QVERIFY(std::isnan(byId->equatorial.rightAscensionHours));

        const std::optional<CelestialBodyState> byIndex = engine.computeBodyState(request, std::size_t{0});
        QVERIFY(byIndex.has_value());
        QCOMPARE(
            static_cast<std::uint8_t>(byIndex->metadata.status),
            static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
        );
        QVERIFY(std::isnan(byIndex->equatorial.rightAscensionHours));
    }
}

void EphemerisRequestTimeContractTests::simpleEngineRejectsNonFiniteExplicitEpoch()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeSunBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    const SimpleEphemerisEngine engine(catalog, SimpleEphemerisEngine::defaultOptions());

    const UtcTimePoint staleUtcTime = utcTime(0);
    const std::array<double, 3> nonFiniteValues{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const double nonFinite : nonFiniteValues) {
        for (const bool nonFiniteInPart1 : {false, true}) {
            EphemerisRequest request;
            request.context = makeContext(staleUtcTime);
            request.epoch = AstronomicalEpoch{
                .julianDatePart1 = nonFiniteInPart1 ? nonFinite : 0.0,
                .julianDatePart2 = nonFiniteInPart1 ? 0.0 : nonFinite,
                .timeScale = TimeScale::Utc,
            };
            request.options = engine.options();

            const EphemerisSnapshot snapshot = engine.compute(request);
            QCOMPARE(snapshot.states.size(), std::size_t{1});
            const CelestialBodyState& state = snapshot.states.front();
            QCOMPARE(
                static_cast<std::uint8_t>(state.metadata.status),
                static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
            );
            QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::InvalidExplicitEpoch));
            QVERIFY(!state.metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion));
            QVERIFY(!state.metadata.isSuccessful());
            QVERIFY(std::isnan(state.equatorial.rightAscensionHours));

            const std::optional<CelestialBodyState> byId = engine.computeBodyState(request, "sun");
            QVERIFY(byId.has_value());
            QCOMPARE(
                static_cast<std::uint8_t>(byId->metadata.status),
                static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
            );
            QVERIFY(byId->metadata.hasWarning(EphemerisEngineWarning::Code::InvalidExplicitEpoch));
            QVERIFY(!byId->metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion));
            QVERIFY(!byId->metadata.isSuccessful());
            QVERIFY(std::isnan(byId->equatorial.rightAscensionHours));

            const std::optional<CelestialBodyState> byIndex = engine.computeBodyState(request, std::size_t{0});
            QVERIFY(byIndex.has_value());
            QCOMPARE(
                static_cast<std::uint8_t>(byIndex->metadata.status),
                static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
            );
            QVERIFY(byIndex->metadata.hasWarning(EphemerisEngineWarning::Code::InvalidExplicitEpoch));
            QVERIFY(!byIndex->metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion));
            QVERIFY(!byIndex->metadata.isSuccessful());
            QVERIFY(std::isnan(byIndex->equatorial.rightAscensionHours));
        }
    }
}

void EphemerisRequestTimeContractTests::simpleEngineUsesExplicitUtcEpochOverStaleContext()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeSunBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    const SimpleEphemerisEngine engine(catalog, SimpleEphemerisEngine::defaultOptions());

    const UtcTimePoint epochUtcTime = utcTime(1'704'067'200);
    const UtcTimePoint staleUtcTime = utcTime(0);

    EphemerisRequest request;
    request.context = makeContext(staleUtcTime);
    request.epoch = EpochCodec::epochFromUtcTime(epochUtcTime);
    request.options = engine.options();

    const EphemerisSnapshot requestSnapshot = engine.compute(request);
    const EphemerisSnapshot referenceSnapshot = engine.compute(makeContext(epochUtcTime));

    QCOMPARE(requestSnapshot.states.size(), std::size_t{1});
    QCOMPARE(referenceSnapshot.states.size(), std::size_t{1});
    QVERIFY(requestSnapshot.context.utcTime == epochUtcTime);

    const CelestialBodyState& requestState = requestSnapshot.states.front();
    const CelestialBodyState& referenceState = referenceSnapshot.states.front();
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.rightAscensionHours, referenceState.equatorial.rightAscensionHours, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.declinationDeg, referenceState.equatorial.declinationDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.horizontal.altitudeDeg, referenceState.horizontal.altitudeDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.horizontal.azimuthDeg, referenceState.horizontal.azimuthDeg, 1e-12
        )
    );
}

void EphemerisRequestTimeContractTests::simpleEngineConvenienceOverloadMatchesExplicitUtcRequest()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeSunBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    const SimpleEphemerisEngine engine(catalog, SimpleEphemerisEngine::defaultOptions());

    const ObservationContext context = makeContext(utcTime(1'704'067'200));
    const EphemerisRequest request = EphemerisRequestFactory::requestFromContext(context, engine.options());

    const EphemerisSnapshot contextSnapshot = engine.compute(context);
    const EphemerisSnapshot requestSnapshot = engine.compute(request);

    QCOMPARE(contextSnapshot.states.size(), requestSnapshot.states.size());
    QCOMPARE(requestSnapshot.states.size(), std::size_t{1});

    const CelestialBodyState& contextState = contextSnapshot.states.front();
    const CelestialBodyState& requestState = requestSnapshot.states.front();
    QCOMPARE(contextState.metadata.status, requestState.metadata.status);
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.rightAscensionHours, contextState.equatorial.rightAscensionHours, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.declinationDeg, contextState.equatorial.declinationDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.horizontal.altitudeDeg, contextState.horizontal.altitudeDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(requestState.horizontal.azimuthDeg, contextState.horizontal.azimuthDeg, 1e-12)
    );
}

void EphemerisRequestTimeContractTests::simpleEngineZeroEpochRemainsContextOnly()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeSunBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    const SimpleEphemerisEngine engine(catalog, SimpleEphemerisEngine::defaultOptions());

    const ObservationContext context = makeContext(utcTime(1'704'067'200));

    EphemerisRequest request;
    request.context = context;
    request.options = engine.options();
    // request.epoch stays at its default/zero value, which means "no explicit
    // epoch" and must be treated as a context-only request.

    const EphemerisSnapshot requestSnapshot = engine.compute(request);
    const EphemerisSnapshot contextSnapshot = engine.compute(context);

    QCOMPARE(requestSnapshot.states.size(), std::size_t{1});
    QCOMPARE(contextSnapshot.states.size(), std::size_t{1});

    const CelestialBodyState& requestState = requestSnapshot.states.front();
    const CelestialBodyState& contextState = contextSnapshot.states.front();

    QVERIFY(requestState.metadata.isSuccessful());
    QVERIFY(!requestState.metadata.hasWarning(EphemerisEngineWarning::Code::InvalidExplicitEpoch));
    QCOMPARE(requestState.metadata.status, contextState.metadata.status);
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.rightAscensionHours, contextState.equatorial.rightAscensionHours, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.declinationDeg, contextState.equatorial.declinationDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.horizontal.altitudeDeg, contextState.horizontal.altitudeDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(requestState.horizontal.azimuthDeg, contextState.horizontal.azimuthDeg, 1e-12)
    );
}

#ifdef SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS

void EphemerisRequestTimeContractTests::highPrecisionEngineUsesExplicitUtcEpoch()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeFixedStarBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    auto calculator = std::make_shared<RecordingStarAstrometryCalculator>();
    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(catalog, calculator);

    const UtcTimePoint epochUtcTime = utcTime(1'704'067'200);
    const UtcTimePoint staleUtcTime = utcTime(0);

    EphemerisRequest request;
    request.context = makeContext(staleUtcTime);
    request.epoch = EpochCodec::epochFromUtcTime(epochUtcTime);
    request.options = engine.options();

    const std::optional<CelestialBodyState> state = engine.computeBodyState(request, "vega");

    QVERIFY(state.has_value());
    QVERIFY(!calculator->empty());
    QVERIFY(calculator->epochs().back() == request.epoch);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );
}

void EphemerisRequestTimeContractTests::highPrecisionEngineUsesExplicitTdbEpoch()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeFixedStarBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    auto calculator = std::make_shared<RecordingStarAstrometryCalculator>();
    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(catalog, calculator);

    EphemerisRequest request;
    request.context = makeContext(utcTime(0));
    request.epoch = AstronomicalEpoch{
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options = engine.options();

    const std::optional<CelestialBodyState> state = engine.computeBodyState(request, "vega");

    QVERIFY(state.has_value());
    QVERIFY(!calculator->empty());
    QVERIFY(calculator->epochs().back() == request.epoch);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );
}

void EphemerisRequestTimeContractTests::highPrecisionEngineConvenienceOverloadBuildsUtcEpoch()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeFixedStarBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    auto calculator = std::make_shared<RecordingStarAstrometryCalculator>();
    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(catalog, calculator);

    const ObservationContext context = makeContext(utcTime(1'704'067'200));
    const EphemerisSnapshot contextSnapshot = engine.compute(context);

    QCOMPARE(contextSnapshot.states.size(), std::size_t{1});
    QVERIFY(!calculator->empty());
    const AstronomicalEpoch contextEpoch = calculator->epochs().back();
    QVERIFY(contextEpoch.timeScale == TimeScale::Utc);
    QVERIFY(contextEpoch == EpochCodec::epochFromUtcTime(context.utcTime));

    const EphemerisRequest request = EphemerisRequestFactory::requestFromContext(context, engine.options());
    const EphemerisSnapshot requestSnapshot = engine.compute(request);

    QCOMPARE(requestSnapshot.states.size(), std::size_t{1});
    QVERIFY(calculator->epochs().size() >= 2U);
    QVERIFY(calculator->epochs().back() == contextEpoch);
}

void EphemerisRequestTimeContractTests::highPrecisionEngineRejectsNonExplicitEpoch()
{
    const std::array<OwnGalaxyCelestialBody, 1> bodies{makeFixedStarBody()};
    const CelestialBodyCatalog catalog(std::span<const OwnGalaxyCelestialBody>{bodies});
    auto calculator = std::make_shared<RecordingStarAstrometryCalculator>();
    const HighPrecisionEphemerisEngine engine = makeHighPrecisionEngine(catalog, calculator);

    EphemerisRequest request;
    request.context = makeContext(utcTime(1'704'067'200));
    request.options = engine.options();

    const std::optional<CelestialBodyState> state = engine.computeBodyState(request, "vega");

    QVERIFY(state.has_value());
    QVERIFY(calculator->empty());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
}

#endif

QTEST_APPLESS_MAIN(EphemerisRequestTimeContractTests)

#include "EphemerisRequestTimeContractTests.moc"
