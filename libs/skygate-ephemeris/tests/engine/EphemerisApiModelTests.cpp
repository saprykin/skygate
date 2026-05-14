#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] skygate::ephemeris::CelestialBody makeFactoryTestBody()
{
    return {
        .id = "vega",
        .displayName = "Vega",
        .type = skygate::ephemeris::CelestialBodyType::Star,
        .fixedEquatorial =
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 18.6156,
                .declinationDeg = 38.7837,
            },
    };
}

class TestStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit TestStarCatalog(std::vector<skygate::ephemeris::CelestialBody> bodies) : m_bodies(std::move(bodies)) {}

    [[nodiscard]] std::span<const skygate::ephemeris::CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    std::vector<skygate::ephemeris::CelestialBody> m_bodies;
};

}  // namespace

class EphemerisApiModelTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactlyTwoEngineKinds();
    void constructsHighPrecisionModelDefaults();
    void combinesCorrectionFlags();
    void constructsRequestAndDataSetModels();
    void constructsCatalogStarAstrometryModel();
    void constructsAndNormalizesAstronomicalTimePrimitives();
    void convertsCivilDatesAndDefinesNoYearZeroPolicy();
    void constructsFactoryRequestDefaults();
    void constructsSimpleAndHighPrecisionFactoryRequests();
    void exposesFactoryFallbackPolicyHelpers();
    void constructsFactoryResultAndCreationDiagnostics();
    void preservesSimpleFactoryCompatibilityOverloads();
    void constructsResultStatusAndWarningModels();
    void keepsLegacyBodyStateFieldsReadableWithMetadata();
    void simpleEngineExposesMetadataDefaults();
};

void EphemerisApiModelTests::exposesExactlyTwoEngineKinds()
{
    constexpr std::array<skygate::ephemeris::EphemerisEngineKind, skygate::ephemeris::ephemerisEngineKindCount()>
        engineKinds{
            skygate::ephemeris::EphemerisEngineKind::Simple,
            skygate::ephemeris::EphemerisEngineKind::HighPrecision,
        };

    QCOMPARE(engineKinds.size(), 2U);
    QVERIFY(skygate::ephemeris::displayName(engineKinds[0]) == "Simple");
    QVERIFY(skygate::ephemeris::displayName(engineKinds[1]) == "High precision");
}

void EphemerisApiModelTests::constructsHighPrecisionModelDefaults()
{
    skygate::ephemeris::EphemerisEngineOptions options;
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(options.fallbackToSimpleEngine);
    QVERIFY(options.enableAtmosphericRefraction);
    QVERIFY(options.atmosphericPressureHpa > 0.0);
    QVERIFY(options.observingWavelengthMicrometers > 0.0);
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
    ));

    skygate::ephemeris::EphemerisCapabilities capabilities;
    QCOMPARE(
        static_cast<std::uint8_t>(capabilities.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(capabilities.supportedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    QVERIFY(!capabilities.supportsSolarSystemBodies);
    QVERIFY(!capabilities.supportsExtendedHistoricalRange);
}

void EphemerisApiModelTests::combinesCorrectionFlags()
{
    auto flags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                 | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration;
    flags |= skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax;

    QVERIFY(skygate::ephemeris::hasCorrectionFlag(flags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime));
    QVERIFY(
        skygate::ephemeris::hasCorrectionFlag(flags, skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration)
    );
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(flags, skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax)
    );
    QVERIFY(!skygate::ephemeris::hasCorrectionFlag(
        flags, skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
    ));

    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        skygate::ephemeris::EphemerisCorrectionFlags::Astrometric,
        skygate::ephemeris::EphemerisCorrectionFlags::LightTime
    ));
    QVERIFY(!skygate::ephemeris::hasCorrectionFlag(
        skygate::ephemeris::EphemerisCorrectionFlags::Astrometric,
        skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax
    ));
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric,
        skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
    ));
}

void EphemerisApiModelTests::constructsRequestAndDataSetModels()
{
    skygate::ephemeris::AstronomicalEpoch epoch;
    epoch.julianDatePart1 = 2'460'310.0;
    epoch.julianDatePart2 = 0.5;
    epoch.timeScale = skygate::ephemeris::TimeScale::Tt;

    skygate::ephemeris::EphemerisRequest request;
    request.epoch = epoch;
    request.context.observer.latitudeDeg = 37.7749;
    request.context.observer.longitudeDeg = -122.4194;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;

    QCOMPARE(
        static_cast<std::uint8_t>(request.epoch.timeScale), static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tt)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(request.context.utcTime.time_since_epoch().count(), 1'704'067'200);

    skygate::ephemeris::EphemerisDateRange modernRange;
    modernRange.id = "modern";
    modernRange.displayName = "Modern kernel";
    modernRange.start = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
    };
    modernRange.end = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'600'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
    };

    skygate::ephemeris::EphemerisDataSetInfo dataSet;
    dataSet.id = "de440s";
    dataSet.displayName = "DE440s";
    dataSet.version = "test";
    dataSet.provenance = "JPL";
    dataSet.dateRanges.push_back(modernRange);

    QCOMPARE(dataSet.dateRanges.size(), std::size_t{1});
    QVERIFY(dataSet.dateRanges.front().id == std::string("modern"));
    QCOMPARE(
        static_cast<std::uint8_t>(dataSet.dateRanges.front().start.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tdb)
    );
}

void EphemerisApiModelTests::constructsCatalogStarAstrometryModel()
{
    skygate::ephemeris::CelestialBody body = makeFactoryTestBody();
    body.starAstrometry = skygate::ephemeris::CatalogStarAstrometry{
        .referenceEquatorial = *body.fixedEquatorial,
        .referenceEpoch =
            {
                .julianDatePart1 = 2'451'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = skygate::ephemeris::TimeScale::Tt,
            },
        .properMotionRightAscensionMasPerYear = 200.0,
        .properMotionDeclinationMasPerYear = -300.0,
        .stellarParallaxMas = 130.0,
        .radialVelocityKmPerSecond = -13.9,
    };

    QVERIFY(body.starAstrometry.has_value());
    QCOMPARE(body.starAstrometry->referenceEquatorial.rightAscensionHours, 18.6156);
    QCOMPARE(body.starAstrometry->properMotionRightAscensionMasPerYear.value_or(0.0), 200.0);
    QCOMPARE(body.starAstrometry->properMotionDeclinationMasPerYear.value_or(0.0), -300.0);
    QCOMPARE(body.starAstrometry->stellarParallaxMas.value_or(0.0), 130.0);
    QCOMPARE(body.starAstrometry->radialVelocityKmPerSecond.value_or(0.0), -13.9);
}

void EphemerisApiModelTests::constructsAndNormalizesAstronomicalTimePrimitives()
{
    const skygate::ephemeris::AstronomicalEpoch overflowEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 1.75,
        .timeScale = skygate::ephemeris::TimeScale::Tt,
    };
    const auto normalizedOverflow = skygate::ephemeris::normalizedAstronomicalEpoch(overflowEpoch);
    QCOMPARE(normalizedOverflow.julianDatePart1, 2'451'546.0);
    QCOMPARE(normalizedOverflow.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(normalizedOverflow.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tt)
    );

    const skygate::ephemeris::AstronomicalEpoch negativeFractionEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = -0.25,
        .timeScale = skygate::ephemeris::TimeScale::Tdb,
    };
    const auto normalizedNegativeFraction = skygate::ephemeris::normalizedAstronomicalEpoch(negativeFractionEpoch);
    QCOMPARE(normalizedNegativeFraction.julianDatePart1, 2'451'544.0);
    QCOMPARE(normalizedNegativeFraction.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(normalizedNegativeFraction.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tdb)
    );

    const skygate::ephemeris::CivilDateTime j2000Noon{
        .astronomicalYear = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .timeScale = skygate::ephemeris::TimeScale::Tt,
    };
    const auto j2000Epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(j2000Noon);
    QVERIFY(j2000Epoch.has_value());
    QCOMPARE(j2000Epoch->julianDatePart1, 2'451'545.0);
    QCOMPARE(j2000Epoch->julianDatePart2, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(j2000Epoch->timeScale), static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tt)
    );
}

void EphemerisApiModelTests::convertsCivilDatesAndDefinesNoYearZeroPolicy()
{
    const skygate::ephemeris::CivilDateTime preciseDateTime{
        .astronomicalYear = 2026,
        .month = 5,
        .day = 13,
        .hour = 12,
        .minute = 34,
        .second = 56,
        .nanosecond = 123'456'789U,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };

    const auto preciseEpoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(preciseDateTime);
    QVERIFY(preciseEpoch.has_value());
    const auto preciseRoundTrip = skygate::ephemeris::civilDateTimeFromAstronomicalEpoch(*preciseEpoch);
    QVERIFY(preciseRoundTrip.has_value());
    QCOMPARE(preciseRoundTrip->astronomicalYear, preciseDateTime.astronomicalYear);
    QCOMPARE(preciseRoundTrip->month, preciseDateTime.month);
    QCOMPARE(preciseRoundTrip->day, preciseDateTime.day);
    QCOMPARE(preciseRoundTrip->hour, preciseDateTime.hour);
    QCOMPARE(preciseRoundTrip->minute, preciseDateTime.minute);
    QCOMPARE(preciseRoundTrip->second, preciseDateTime.second);
    const std::uint32_t nanosecondDelta = preciseRoundTrip->nanosecond > preciseDateTime.nanosecond
                                              ? preciseRoundTrip->nanosecond - preciseDateTime.nanosecond
                                              : preciseDateTime.nanosecond - preciseRoundTrip->nanosecond;
    QVERIFY(nanosecondDelta <= 1'000U);

    const skygate::ephemeris::CivilDateTime astronomicalYearZero{
        .astronomicalYear = 0,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };
    const auto yearZeroEpoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(astronomicalYearZero);
    QVERIFY(yearZeroEpoch.has_value());
    const auto yearZeroRoundTrip = skygate::ephemeris::civilDateTimeFromAstronomicalEpoch(*yearZeroEpoch);
    QVERIFY(yearZeroRoundTrip.has_value());
    QCOMPARE(yearZeroRoundTrip->astronomicalYear, 0);
    QCOMPARE(yearZeroRoundTrip->month, 1);
    QCOMPARE(yearZeroRoundTrip->day, 1);

    const auto astronomicalFromOneBce = skygate::ephemeris::astronomicalYearFromHistoricalYear(-1);
    QVERIFY(astronomicalFromOneBce.has_value());
    QCOMPARE(*astronomicalFromOneBce, 0);
    const auto astronomicalFromFortyFourBce = skygate::ephemeris::astronomicalYearFromHistoricalYear(-44);
    QVERIFY(astronomicalFromFortyFourBce.has_value());
    QCOMPARE(*astronomicalFromFortyFourBce, -43);
    QVERIFY(!skygate::ephemeris::astronomicalYearFromHistoricalYear(0).has_value());
    QCOMPARE(skygate::ephemeris::historicalYearFromAstronomicalYear(0), -1);
    QCOMPARE(skygate::ephemeris::historicalYearFromAstronomicalYear(-43), -44);
    QCOMPARE(skygate::ephemeris::historicalYearFromAstronomicalYear(2026), 2026);

    const skygate::ephemeris::CivilDateTime invalidLeapDay{
        .astronomicalYear = 2023,
        .month = 2,
        .day = 29,
    };
    QVERIFY(!skygate::ephemeris::astronomicalEpochFromCivilDateTime(invalidLeapDay).has_value());

    const skygate::ephemeris::CivilDateTime leapSecondLabel{
        .astronomicalYear = 2016,
        .month = 12,
        .day = 31,
        .hour = 23,
        .minute = 59,
        .second = 60,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };
    QVERIFY(skygate::ephemeris::isValidCivilDateTime(leapSecondLabel));
    QVERIFY(!skygate::ephemeris::astronomicalEpochFromCivilDateTime(leapSecondLabel).has_value());
}

void EphemerisApiModelTests::constructsFactoryRequestDefaults()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;

    QCOMPARE(
        static_cast<std::uint8_t>(request.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(request.catalogBodies.size(), std::size_t{0});
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.fallbackPolicy),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision)
    );
    QVERIFY(!skygate::ephemeris::allowsSimpleEngineFallback(request.fallbackPolicy));
    QVERIFY(request.dataSetManifest == nullptr);
    QVERIFY(request.activeDataSnapshot == nullptr);
    QVERIFY(request.timeScaleService == nullptr);
    QVERIFY(request.earthOrientationProvider == nullptr);
    QVERIFY(request.diagnosticsSink == nullptr);
    QVERIFY(request.dataManifest == nullptr);
    QVERIFY(request.calcephKernelRuntime == nullptr);
}

void EphemerisApiModelTests::constructsSimpleAndHighPrecisionFactoryRequests()
{
    const std::array bodies{makeFactoryTestBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest simpleRequest;
    simpleRequest.catalogBodies = bodies;

    QCOMPARE(simpleRequest.catalogBodies.size(), std::size_t{1});
    QVERIFY(simpleRequest.catalogBodies.front().id == std::string{"vega"});
    QCOMPARE(
        static_cast<std::uint8_t>(simpleRequest.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );

    skygate::ephemeris::EphemerisDataSetInfo manifest;
    manifest.id = "de440s";
    manifest.displayName = "DE440s";

    skygate::ephemeris::EphemerisEngineFactoryRequest highPrecisionRequest;
    highPrecisionRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    highPrecisionRequest.catalogBodies = bodies;
    highPrecisionRequest.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    highPrecisionRequest.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent;
    highPrecisionRequest.dataSetManifest = &manifest;
    highPrecisionRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;

    QCOMPARE(
        static_cast<std::uint8_t>(highPrecisionRequest.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(highPrecisionRequest.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(highPrecisionRequest.options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::Apparent)
    );
    QVERIFY(highPrecisionRequest.dataSetManifest == &manifest);
    QVERIFY(!skygate::ephemeris::allowsSimpleEngineFallback(highPrecisionRequest.fallbackPolicy));
}

void EphemerisApiModelTests::exposesFactoryFallbackPolicyHelpers()
{
    constexpr auto strictPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    constexpr auto fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    QVERIFY(!skygate::ephemeris::allowsSimpleEngineFallback(strictPolicy));
    QVERIFY(skygate::ephemeris::allowsSimpleEngineFallback(fallbackPolicy));
    QVERIFY(skygate::ephemeris::displayName(strictPolicy) == std::string_view{"strict high precision"});
    QVERIFY(skygate::ephemeris::displayName(fallbackPolicy) == std::string_view{"allow simple engine fallback"});
}

void EphemerisApiModelTests::constructsFactoryResultAndCreationDiagnostics()
{
    constexpr std::array factoryStatuses{
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine,
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedCreationError,
    };

    for (const skygate::ephemeris::EphemerisFactoryCreationStatus status : factoryStatuses) {
        QVERIFY(!skygate::ephemeris::displayName(status).empty());
    }

    QVERIFY(skygate::ephemeris::isFactoryCreationSuccess(
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine
    ));
    QVERIFY(skygate::ephemeris::isFactoryCreationSuccess(
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback
    ));
    QVERIFY(!skygate::ephemeris::isFactoryCreationSuccess(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
    ));

    const std::array diagnosticCodes{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::RequiredTimeScaleServiceUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::RequiredEarthOrientationProviderUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed,
    };

    for (const skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode code : diagnosticCodes) {
        QVERIFY(!skygate::ephemeris::ephemerisFactoryCreationDiagnosticText(code).empty());
    }

    skygate::ephemeris::EphemerisFactoryCreationDiagnostic fallbackDiagnostic{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    QVERIFY(!fallbackDiagnostic.isError());
    QVERIFY(!fallbackDiagnostic.displayText().empty());

    skygate::ephemeris::EphemerisFactoryCreationDiagnostic strictFailureDiagnostic{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "strict high precision requested but unavailable",
    };
    QVERIFY(strictFailureDiagnostic.isError());
    QVERIFY(
        strictFailureDiagnostic.displayText() == std::string_view{"strict high precision requested but unavailable"}
    );

    auto successResult =
        skygate::ephemeris::EphemerisEngineFactoryResult::success(skygate::ephemeris::createEphemerisEngine());
    QVERIFY(successResult.isSuccess());
    QVERIFY(!successResult.isFailure());
    QVERIFY(successResult.engine != nullptr);
    QVERIFY(!successResult.usedSimpleEngineFallback());
    QVERIFY(!successResult.hasDiagnostics());
    QVERIFY(!successResult.hasErrors());

    auto fallbackResult = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        skygate::ephemeris::createEphemerisEngine(),
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        {fallbackDiagnostic}
    );
    QVERIFY(fallbackResult.isSuccess());
    QVERIFY(fallbackResult.engine != nullptr);
    QVERIFY(fallbackResult.usedSimpleEngineFallback());
    QVERIFY(fallbackResult.hasDiagnostics());
    QVERIFY(!fallbackResult.hasErrors());
    QCOMPARE(fallbackResult.diagnostics.size(), std::size_t{1});

    auto strictFailureResult = skygate::ephemeris::EphemerisEngineFactoryResult::failure(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable,
        {strictFailureDiagnostic}
    );
    QVERIFY(!strictFailureResult.isSuccess());
    QVERIFY(strictFailureResult.isFailure());
    QVERIFY(strictFailureResult.engine == nullptr);
    QVERIFY(!strictFailureResult.usedSimpleEngineFallback());
    QVERIFY(strictFailureResult.hasDiagnostics());
    QVERIFY(strictFailureResult.hasErrors());
    QCOMPARE(strictFailureResult.diagnostics.size(), std::size_t{1});
}

void EphemerisApiModelTests::preservesSimpleFactoryCompatibilityOverloads()
{
    skygate::core::SkyContext context;
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto emptyEngine = skygate::ephemeris::createEphemerisEngine();
    QVERIFY(emptyEngine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(emptyEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(emptyEngine->compute(context).states.empty());

    const auto emptyBraceEngine = skygate::ephemeris::createEphemerisEngine({});
    QVERIFY(emptyBraceEngine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(emptyBraceEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(emptyBraceEngine->compute(context).states.empty());

    const std::array bodies{makeFactoryTestBody()};
    const auto spanEngine =
        skygate::ephemeris::createEphemerisEngine(std::span<const skygate::ephemeris::CelestialBody>{
            bodies.data(),
            bodies.size(),
        });
    QVERIFY(spanEngine != nullptr);

    const auto spanState = spanEngine->computeBodyState(context, "vega");
    QVERIFY(spanState.has_value());
    QCOMPARE(spanState->bodyIndex, 0U);
    QCOMPARE(spanState->equatorial.rightAscensionHours, 18.6156);
    QCOMPARE(spanState->equatorial.declinationDeg, 38.7837);

    const TestStarCatalog catalog({makeFactoryTestBody()});
    const auto catalogEngine = skygate::ephemeris::createEphemerisEngine(catalog);
    QVERIFY(catalogEngine != nullptr);

    const auto catalogState = catalogEngine->computeBodyState(context, std::uint32_t{0});
    QVERIFY(catalogState.has_value());
    QCOMPARE(catalogState->bodyIndex, 0U);
    QCOMPARE(catalogState->equatorial.rightAscensionHours, spanState->equatorial.rightAscensionHours);
    QCOMPARE(catalogState->equatorial.declinationDeg, spanState->equatorial.declinationDeg);

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.catalogBodies = bodies;
    const auto requestResult = skygate::ephemeris::createEphemerisEngine(request);
    QVERIFY(requestResult.isSuccess());
    QVERIFY(requestResult.engine != nullptr);
    QVERIFY(!requestResult.usedSimpleEngineFallback());
    QVERIFY(!requestResult.hasDiagnostics());
    QCOMPARE(
        static_cast<std::uint8_t>(requestResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
}

void EphemerisApiModelTests::constructsResultStatusAndWarningModels()
{
    constexpr std::array<skygate::ephemeris::EphemerisResultStatus, skygate::ephemeris::ephemerisResultStatusCount()>
        statuses{
            skygate::ephemeris::EphemerisResultStatus::Valid,
            skygate::ephemeris::EphemerisResultStatus::Degraded,
            skygate::ephemeris::EphemerisResultStatus::Unsupported,
            skygate::ephemeris::EphemerisResultStatus::OutOfRange,
            skygate::ephemeris::EphemerisResultStatus::Failed,
        };

    QCOMPARE(statuses.size(), 5U);
    QVERIFY(skygate::ephemeris::displayName(statuses[0]) == "valid");
    QVERIFY(skygate::ephemeris::displayName(statuses[1]) == "degraded");
    QVERIFY(skygate::ephemeris::displayName(statuses[2]) == "unsupported");
    QVERIFY(skygate::ephemeris::displayName(statuses[3]) == "out of range");
    QVERIFY(skygate::ephemeris::displayName(statuses[4]) == "failed");

    const std::array warningCodes{
        skygate::ephemeris::EphemerisWarningCode::AccuracyDegraded,
        skygate::ephemeris::EphemerisWarningCode::UnsupportedBody,
        skygate::ephemeris::EphemerisWarningCode::DataOutOfRange,
        skygate::ephemeris::EphemerisWarningCode::ComputationFailed,
    };

    for (const skygate::ephemeris::EphemerisWarningCode code : warningCodes) {
        const skygate::ephemeris::EphemerisWarning warning(code);
        QVERIFY(!warning.displayText().empty());
        QVERIFY(!skygate::ephemeris::ephemerisWarningText(code).empty());
    }

    const skygate::ephemeris::EphemerisWarning fallbackTextWarning(
        skygate::ephemeris::EphemerisWarningCode::DataOutOfRange
    );
    QVERIFY(!fallbackTextWarning.displayText().empty());

    skygate::ephemeris::EphemerisResultMetadata metadata;
    QVERIFY(metadata.isSuccessful());
    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisResultStatus::Valid)
    );
    QCOMPARE(metadata.warningCount(), std::size_t{0});
    QVERIFY(!metadata.hasWarnings());
    QVERIFY(!metadata.effectiveDataValidityRange.has_value());
    QVERIFY(!metadata.estimatedAngularUncertaintyArcsec.has_value());

    skygate::ephemeris::EphemerisDateRange validityRange;
    validityRange.id = "modern";
    validityRange.displayName = "Modern kernel";

    metadata.status = skygate::ephemeris::EphemerisResultStatus::Failed;
    metadata.addWarning(skygate::ephemeris::EphemerisWarningCode::ComputationFailed);
    metadata.dataSourceProvenance = "test source";
    metadata.effectiveDataValidityRange = validityRange;
    metadata.appliedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
    metadata.addUnavailableCorrection(skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration);
    metadata.finalizeCorrectionTracking(
        skygate::ephemeris::EphemerisCorrectionFlags::LightTime
        | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration
        | skygate::ephemeris::EphemerisCorrectionFlags::GravitationalLightDeflection
    );

    QVERIFY(!metadata.isSuccessful());
    QCOMPARE(metadata.warningCount(), std::size_t{2});
    QVERIFY(metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::ComputationFailed));
    QVERIFY(metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
    QVERIFY(metadata.effectiveDataValidityRange.has_value());
    QVERIFY(metadata.effectiveDataValidityRange->id == std::string{"modern"});
    QVERIFY(metadata.dataSourceProvenance == std::string{"test source"});
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        metadata.appliedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
    ));
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        metadata.unavailableCorrections, skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration
    ));
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        metadata.skippedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::GravitationalLightDeflection
    ));
}

void EphemerisApiModelTests::keepsLegacyBodyStateFieldsReadableWithMetadata()
{
    skygate::ephemeris::CelestialBodyState state{
        .bodyIndex = 42U,
        .equatorial =
            {
                .rightAscensionHours = 12.5,
                .declinationDeg = -4.0,
            },
        .horizontal =
            {
                .altitudeDeg = 30.0,
                .azimuthDeg = 180.0,
            },
    };

    QCOMPARE(state.bodyIndex, 42U);
    QCOMPARE(state.equatorial.rightAscensionHours, 12.5);
    QCOMPARE(state.equatorial.declinationDeg, -4.0);
    QCOMPARE(state.horizontal.altitudeDeg, 30.0);
    QCOMPARE(state.horizontal.azimuthDeg, 180.0);
    QCOMPARE(
        static_cast<std::uint8_t>(state.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisResultStatus::Valid)
    );
    QCOMPARE(state.metadata.warningCount(), std::size_t{0});
}

void EphemerisApiModelTests::simpleEngineExposesMetadataDefaults()
{
    const auto engine = skygate::ephemeris::createEphemerisEngine();
    QVERIFY(engine != nullptr);

    QCOMPARE(
        static_cast<std::uint8_t>(engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(engine->name() == std::string_view{"Simple ephemeris engine"});

    const auto capabilities = engine->capabilities();
    QCOMPARE(
        static_cast<std::uint8_t>(capabilities.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(capabilities.supportedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    QVERIFY(capabilities.supportsSolarSystemBodies);
    QVERIFY(capabilities.supportsCatalogStars);
    QVERIFY(capabilities.supportsTopocentricPositions);
    QVERIFY(!capabilities.supportsAtmosphericRefraction);
    QVERIFY(!capabilities.supportsExtendedHistoricalRange);

    const auto options = engine->options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    QVERIFY(options.fallbackToSimpleEngine);
    QVERIFY(!options.enableAtmosphericRefraction);

    const auto dataSetInfo = engine->dataSetInfo();
    QVERIFY(dataSetInfo.id == std::string{"simple"});
    QVERIFY(dataSetInfo.displayName == std::string{"Simple ephemeris engine"});
    QVERIFY(dataSetInfo.version == std::string{"built-in"});
    QVERIFY(!dataSetInfo.provenance.empty());
    QVERIFY(dataSetInfo.dateRanges.empty());
    QVERIFY(engine->supportedDateRanges().empty());
}

static_assert(std::is_enum_v<skygate::ephemeris::EphemerisEngineKind>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisCorrectionFlags>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisResultStatus>);
static_assert(sizeof(skygate::ephemeris::EphemerisResultMetadata) <= 192);

QTEST_MAIN(EphemerisApiModelTests)

#include "EphemerisApiModelTests.moc"
