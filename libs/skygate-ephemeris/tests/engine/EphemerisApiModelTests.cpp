#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequestFactory.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "UtcTimeCodec.hpp"
#include "engine/highprecision/CelestialFrameTransformResult.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryTestBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "vega";
    body.displayName = "Vega";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = 18.6156,
        .declinationDeg = 38.7837,
    };
    return body;
}

class TestStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit TestStarCatalog(std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies)
        : m_catalog(std::move(bodies))
    {
    }

    [[nodiscard]] const skygate::ephemeris::CelestialBodyCatalog& catalog() const noexcept override
    {
        return m_catalog;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies() const override
    {
        return m_catalog.bodies();
    }

private:
    skygate::ephemeris::CelestialBodyCatalog m_catalog;
};

}  // namespace

class EphemerisApiModelTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactlyTwoEngineKinds();
    void constructsHighPrecisionModelDefaults();
    void combinesCorrectionFlags();
    void constructsRequestAndDataSetModels();
    void constructsRequestsWithFactory();
    void constructsCatalogStarAstrometryModel();
    void constructsAndNormalizesAstronomicalTimePrimitives();
    void exposesCelestialReferenceFrameHelpers();
    void constructsCelestialFrameTransformResults();
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
    constexpr std::array<
        skygate::ephemeris::EphemerisEngineKind::Type,
        static_cast<int>(skygate::ephemeris::EphemerisEngineKind::Type::Last)>
        engineKinds{
            skygate::ephemeris::EphemerisEngineKind::Type::Simple,
            skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision,
        };

    QCOMPARE(engineKinds.size(), 2U);
    QVERIFY(skygate::ephemeris::EphemerisEngineKind::displayName(engineKinds[0]) == "Simple");
    QVERIFY(skygate::ephemeris::EphemerisEngineKind::displayName(engineKinds[1]) == "High precision");
}

void EphemerisApiModelTests::constructsHighPrecisionModelDefaults()
{
    skygate::ephemeris::EphemerisEngineOptions options;
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(options.fallbackToSimpleEngine());
    QVERIFY(options.enableAtmosphericRefraction());
    QVERIFY(options.atmosphericPressureHpa() > 0.0);
    QVERIFY(options.observingWavelengthMicrometers() > 0.0);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            options.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );

    skygate::ephemeris::EphemerisCapabilities capabilities =
        skygate::ephemeris::EphemerisCapabilities::noCapabilities();
    QVERIFY(capabilities == skygate::ephemeris::EphemerisCapabilities::noCapabilities());
    QVERIFY(!skygate::ephemeris::EphemerisCapabilities::has(
        capabilities, skygate::ephemeris::EphemerisCapabilities::extendedHistoricalRange()
    ));
}

void EphemerisApiModelTests::combinesCorrectionFlags()
{
    auto flags = skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
                 | skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration();
    flags |= skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax();

    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            flags, skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            flags, skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            flags, skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        flags, skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    ));

    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            skygate::ephemeris::EphemerisCorrectionFlags::astrometric(),
            skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        skygate::ephemeris::EphemerisCorrectionFlags::astrometric(),
        skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax()
    ));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric(),
            skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
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
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);

    QCOMPARE(
        static_cast<std::uint8_t>(request.epoch.timeScale), static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tt)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(request.context.utcTime), 1'704'067'200'000'000LL);

    skygate::ephemeris::EphemerisDateRange modernRange;
    modernRange.id = "modern";
    modernRange.displayName = "Modern kernel";
    modernRange.start = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
    };
    modernRange.end = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'600'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
    };

    skygate::ephemeris::EphemerisDatasetInfo dataSet;
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

void EphemerisApiModelTests::constructsRequestsWithFactory()
{
    skygate::core::ObservationContext context;
    context.observer = {.latitudeDeg = 47.3769, .longitudeDeg = 8.5417, .elevationMeters = 408.0};
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    skygate::ephemeris::EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());

    const skygate::ephemeris::EphemerisRequest request =
        skygate::ephemeris::EphemerisRequestFactory::requestFromContext(context, options);
    QVERIFY(request.epoch.hasExplicit());
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        skygate::core::UtcTimeCodec::toEpochMicros(
            skygate::ephemeris::EphemerisRequestFactory::contextFromRequest(request).utcTime
        ),
        1'704'067'200'000'000LL
    );

    const auto microsShiftedUtc = context.utcTime + std::chrono::microseconds(123'456);
    const skygate::ephemeris::EphemerisRequest microsShiftedRequest =
        skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, microsShiftedUtc);
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(microsShiftedRequest.context.utcTime), 1'704'067'200'123'456LL);
    const qint64 microsRoundTrip = skygate::core::UtcTimeCodec::toEpochMicros(
        skygate::ephemeris::EphemerisRequestFactory::contextFromRequest(microsShiftedRequest).utcTime
    );
    QVERIFY(microsRoundTrip != 1'704'067'200'000'000LL);
    QVERIFY(std::llabs(microsRoundTrip - 1'704'067'200'123'456LL) <= 100);

    const auto shiftedUtc = context.utcTime + std::chrono::seconds(90);
    const skygate::ephemeris::EphemerisRequest shiftedRequest =
        skygate::ephemeris::EphemerisRequestFactory::atUtcTime(request, shiftedUtc);
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(shiftedRequest.context.utcTime), 1'704'067'290'000'000LL);
    const qint64 shiftedRoundTrip = skygate::core::UtcTimeCodec::toEpochMicros(
        skygate::ephemeris::EphemerisRequestFactory::contextFromRequest(shiftedRequest).utcTime
    );
    QVERIFY(std::llabs(shiftedRoundTrip - 1'704'067'290'000'000LL) <= 100);
    QCOMPARE(
        static_cast<std::uint32_t>(shiftedRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::apparent())
    );
}

void EphemerisApiModelTests::constructsCatalogStarAstrometryModel()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body = makeFactoryTestBody();
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
    const auto normalizedOverflow = overflowEpoch.normalized();
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
    const auto normalizedNegativeFraction = negativeFractionEpoch.normalized();
    QCOMPARE(normalizedNegativeFraction.julianDatePart1, 2'451'544.0);
    QCOMPARE(normalizedNegativeFraction.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(normalizedNegativeFraction.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tdb)
    );

    const skygate::ephemeris::AstronomicalEpoch utcEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.25,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };
    const auto plusSeconds = utcEpoch.addSeconds(43'200.0);
    QCOMPARE(plusSeconds.julianDatePart1, 2'451'545.0);
    QCOMPARE(plusSeconds.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(plusSeconds.timeScale), static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Utc)
    );

    const auto plusMinutesInTai = utcEpoch.addMinutes(720.0, skygate::ephemeris::TimeScale::Tai);
    QCOMPARE(plusMinutesInTai.julianDatePart1, 2'451'545.0);
    QCOMPARE(plusMinutesInTai.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(plusMinutesInTai.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tai)
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
    const auto j2000Epoch = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(j2000Noon);
    QVERIFY(j2000Epoch.has_value());
    QCOMPARE(j2000Epoch->julianDatePart1, 2'451'545.0);
    QCOMPARE(j2000Epoch->julianDatePart2, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(j2000Epoch->timeScale), static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tt)
    );
}

void EphemerisApiModelTests::exposesCelestialReferenceFrameHelpers()
{
    using skygate::ephemeris::highprecision::CelestialReferenceFrame;

    QCOMPARE(CelestialReferenceFrame::rankFromType(CelestialReferenceFrame::Type::Icrs), 0U);
    QCOMPARE(CelestialReferenceFrame::rankFromType(CelestialReferenceFrame::Type::Gcrs), 0U);
    QCOMPARE(CelestialReferenceFrame::rankFromType(CelestialReferenceFrame::Type::TrueEquatorAndEquinox), 1U);
    QCOMPARE(CelestialReferenceFrame::rankFromType(CelestialReferenceFrame::Type::Cirs), 1U);
    QCOMPARE(CelestialReferenceFrame::rankFromType(CelestialReferenceFrame::Type::Tirs), 2U);
    QCOMPARE(CelestialReferenceFrame::rankFromType(CelestialReferenceFrame::Type::Itrs), 3U);

    QCOMPARE(
        static_cast<std::uint8_t>(CelestialReferenceFrame::typeFromRank(0U)),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(CelestialReferenceFrame::typeFromRank(1U)),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(CelestialReferenceFrame::typeFromRank(2U)),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Tirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(CelestialReferenceFrame::typeFromRank(3U)),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Itrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(CelestialReferenceFrame::typeFromRank(99U)),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Gcrs)
    );

    QVERIFY(CelestialReferenceFrame::isGcrsLike(CelestialReferenceFrame::Type::Icrs));
    QVERIFY(CelestialReferenceFrame::isGcrsLike(CelestialReferenceFrame::Type::Gcrs));
    QVERIFY(!CelestialReferenceFrame::isGcrsLike(CelestialReferenceFrame::Type::Cirs));

    QVERIFY(CelestialReferenceFrame::isTerrestrial(CelestialReferenceFrame::Type::Tirs));
    QVERIFY(CelestialReferenceFrame::isTerrestrial(CelestialReferenceFrame::Type::Itrs));
    QVERIFY(!CelestialReferenceFrame::isTerrestrial(CelestialReferenceFrame::Type::Cirs));
}

void EphemerisApiModelTests::constructsCelestialFrameTransformResults()
{
    using skygate::ephemeris::highprecision::CelestialFrameTransformResult;
    using skygate::ephemeris::highprecision::CelestialReferenceFrame;

    constexpr std::string_view kProvenance = "unit-test frame transform";
    const skygate::core::Vector3d vector{1.0, 2.0, 3.0};

    const CelestialFrameTransformResult failed = CelestialFrameTransformResult::makeFailed(
        skygate::ephemeris::EphemerisEngineWarning::Code::ComputationFailed, kProvenance
    );
    QVERIFY(!failed.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(failed.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(failed.metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::ComputationFailed));
    QCOMPARE(failed.metadata.dataSourceProvenance, std::string{kProvenance});
    QCOMPARE(failed.stages.size(), std::size_t{0});

    const CelestialFrameTransformResult sameFrameIdentity = CelestialFrameTransformResult::makeIdentity(
        CelestialReferenceFrame::Type::Gcrs, CelestialReferenceFrame::Type::Gcrs, vector, kProvenance
    );
    QVERIFY(sameFrameIdentity.vector.has_value());
    QCOMPARE(sameFrameIdentity.vector->x, vector.x);
    QCOMPARE(sameFrameIdentity.vector->y, vector.y);
    QCOMPARE(sameFrameIdentity.vector->z, vector.z);
    QCOMPARE(
        static_cast<std::uint8_t>(sameFrameIdentity.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(sameFrameIdentity.metadata.dataSourceProvenance, std::string{kProvenance});
    QCOMPARE(sameFrameIdentity.stages.size(), std::size_t{0});

    const CelestialFrameTransformResult crossFrameIdentity = CelestialFrameTransformResult::makeIdentity(
        CelestialReferenceFrame::Type::Icrs, CelestialReferenceFrame::Type::Gcrs, vector, kProvenance
    );
    QCOMPARE(crossFrameIdentity.stages.size(), std::size_t{1});
    QCOMPARE(
        static_cast<std::uint8_t>(crossFrameIdentity.stages.front().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Icrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(crossFrameIdentity.stages.front().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Gcrs)
    );
    QVERIFY(!crossFrameIdentity.stages.front().applied);
    QCOMPARE(crossFrameIdentity.stages.front().metadata.dataSourceProvenance, std::string{kProvenance});
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

    const auto preciseEpoch = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(preciseDateTime);
    QVERIFY(preciseEpoch.has_value());
    const auto preciseRoundTrip = skygate::ephemeris::CalendarTime::civilDateTimeFromAstronomicalEpoch(*preciseEpoch);
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
    const auto yearZeroEpoch =
        skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(astronomicalYearZero);
    QVERIFY(yearZeroEpoch.has_value());
    const auto yearZeroRoundTrip = skygate::ephemeris::CalendarTime::civilDateTimeFromAstronomicalEpoch(*yearZeroEpoch);
    QVERIFY(yearZeroRoundTrip.has_value());
    QCOMPARE(yearZeroRoundTrip->astronomicalYear, 0);
    QCOMPARE(yearZeroRoundTrip->month, 1);
    QCOMPARE(yearZeroRoundTrip->day, 1);

    const auto astronomicalFromOneBce = skygate::ephemeris::CalendarTime::astronomicalYearFromHistoricalYear(-1);
    QVERIFY(astronomicalFromOneBce.has_value());
    QCOMPARE(*astronomicalFromOneBce, 0);
    const auto astronomicalFromFortyFourBce = skygate::ephemeris::CalendarTime::astronomicalYearFromHistoricalYear(-44);
    QVERIFY(astronomicalFromFortyFourBce.has_value());
    QCOMPARE(*astronomicalFromFortyFourBce, -43);
    QVERIFY(!skygate::ephemeris::CalendarTime::astronomicalYearFromHistoricalYear(0).has_value());
    QCOMPARE(skygate::ephemeris::CalendarTime::historicalYearFromAstronomicalYear(0), -1);
    QCOMPARE(skygate::ephemeris::CalendarTime::historicalYearFromAstronomicalYear(-43), -44);
    QCOMPARE(skygate::ephemeris::CalendarTime::historicalYearFromAstronomicalYear(2026), 2026);

    const skygate::ephemeris::CivilDateTime invalidLeapDay{
        .astronomicalYear = 2023,
        .month = 2,
        .day = 29,
    };
    QVERIFY(!skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(invalidLeapDay).has_value());

    const skygate::ephemeris::CivilDateTime leapSecondLabel{
        .astronomicalYear = 2016,
        .month = 12,
        .day = 31,
        .hour = 23,
        .minute = 59,
        .second = 60,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };
    QVERIFY(skygate::ephemeris::CalendarTime::isValidCivilDateTime(leapSecondLabel));
    QVERIFY(!skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(leapSecondLabel).has_value());
}

void EphemerisApiModelTests::constructsFactoryRequestDefaults()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;

    QCOMPARE(
        static_cast<std::uint8_t>(request.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(request.catalog == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.fallbackPolicy),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision)
    );
    QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::allowsSimpleEngineFallback(request.fallbackPolicy));
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
    auto catalog = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{makeFactoryTestBody()}
    );

    skygate::ephemeris::EphemerisEngineFactoryRequest simpleRequest;
    simpleRequest.catalog = catalog;

    QVERIFY(simpleRequest.catalog != nullptr);
    QCOMPARE(simpleRequest.catalog->size(), std::size_t{1});
    QVERIFY(simpleRequest.catalog->bodyAt(0).id == std::string{"vega"});
    QCOMPARE(
        static_cast<std::uint8_t>(simpleRequest.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    skygate::ephemeris::EphemerisDatasetInfo manifest;
    manifest.id = "de440s";
    manifest.displayName = "DE440s";

    skygate::ephemeris::EphemerisEngineFactoryRequest highPrecisionRequest;
    highPrecisionRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    highPrecisionRequest.catalog = catalog;
    highPrecisionRequest.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    highPrecisionRequest.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());
    highPrecisionRequest.dataSetManifest = &manifest;
    highPrecisionRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;

    QCOMPARE(
        static_cast<std::uint8_t>(highPrecisionRequest.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(highPrecisionRequest.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(highPrecisionRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::apparent())
    );
    QVERIFY(highPrecisionRequest.dataSetManifest == &manifest);
    QVERIFY(
        !skygate::ephemeris::EphemerisEngineFactory::allowsSimpleEngineFallback(highPrecisionRequest.fallbackPolicy)
    );
}

void EphemerisApiModelTests::exposesFactoryFallbackPolicyHelpers()
{
    constexpr auto strictPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    constexpr auto fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::allowsSimpleEngineFallback(strictPolicy));
    QVERIFY(skygate::ephemeris::EphemerisEngineFactory::allowsSimpleEngineFallback(fallbackPolicy));
    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::displayName(strictPolicy)
        == std::string_view{"strict high precision"}
    );
    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::displayName(fallbackPolicy)
        == std::string_view{"allow simple engine fallback"}
    );
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
        QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::displayName(status).empty());
    }

    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::isCreationSuccess(
            skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisEngineFactory::isCreationSuccess(
            skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::isCreationSuccess(
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
        QVERIFY(!skygate::ephemeris::EphemerisEngineFactory::diagnosticText(code).empty());
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

    auto successEngine = skygate::ephemeris::EphemerisEngineFactory::create();
    auto successResult = skygate::ephemeris::EphemerisEngineFactoryResult::success(std::move(successEngine.engine));
    QVERIFY(successResult.isSuccess());
    QVERIFY(!successResult.isFailure());
    QVERIFY(successResult.engine != nullptr);
    QVERIFY(!successResult.usedSimpleEngineFallback());
    QVERIFY(!successResult.hasDiagnostics());
    QVERIFY(!successResult.hasErrors());

    auto fallbackEngine = skygate::ephemeris::EphemerisEngineFactory::create();
    auto fallbackResult = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        std::move(fallbackEngine.engine),
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
    skygate::core::ObservationContext context;
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    auto emptyEngineResult = skygate::ephemeris::EphemerisEngineFactory::create();
    QVERIFY(emptyEngineResult.isSuccess());
    QVERIFY(emptyEngineResult.engine != nullptr);
    const auto& emptyEngine = emptyEngineResult.engine;
    QCOMPARE(
        static_cast<std::uint8_t>(emptyEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(emptyEngine->compute(context).states.empty());

    auto emptyBraceEngineResult = skygate::ephemeris::EphemerisEngineFactory::create({});
    QVERIFY(emptyBraceEngineResult.isSuccess());
    QVERIFY(emptyBraceEngineResult.engine != nullptr);
    const auto& emptyBraceEngine = emptyBraceEngineResult.engine;
    QCOMPARE(
        static_cast<std::uint8_t>(emptyBraceEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(emptyBraceEngine->compute(context).states.empty());

    const std::array bodies{makeFactoryTestBody()};
    const skygate::ephemeris::CelestialBodyCatalog spanCatalog(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
    auto spanEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(spanCatalog);
    QVERIFY(spanEngineResult.isSuccess());
    const auto& spanEngine = spanEngineResult.engine;

    const auto spanState = spanEngine->computeBodyState(context, "vega");
    QVERIFY(spanState.has_value());
    QCOMPARE(spanState->bodyIndex, 0U);
    QCOMPARE(spanState->equatorial.rightAscensionHours, 18.6156);
    QCOMPARE(spanState->equatorial.declinationDeg, 38.7837);

    const TestStarCatalog catalog({makeFactoryTestBody()});
    auto catalogEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(catalog);
    QVERIFY(catalogEngineResult.isSuccess());
    const auto& catalogEngine = catalogEngineResult.engine;

    const auto catalogState = catalogEngine->computeBodyState(context, std::uint32_t{0});
    QVERIFY(catalogState.has_value());
    QCOMPARE(catalogState->bodyIndex, 0U);
    QCOMPARE(catalogState->equatorial.rightAscensionHours, spanState->equatorial.rightAscensionHours);
    QCOMPARE(catalogState->equatorial.declinationDeg, spanState->equatorial.declinationDeg);

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.catalog = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
    const auto requestResult = skygate::ephemeris::EphemerisEngineFactory::create(request);
    QVERIFY(requestResult.isSuccess());
    QVERIFY(requestResult.engine != nullptr);
    QVERIFY(!requestResult.usedSimpleEngineFallback());
    QVERIFY(!requestResult.hasDiagnostics());
    QCOMPARE(
        static_cast<std::uint8_t>(requestResult.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
}

void EphemerisApiModelTests::constructsResultStatusAndWarningModels()
{
    constexpr std::array<
        skygate::ephemeris::EphemerisEngineQueryStatus::Type,
        static_cast<int>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Last)>
        statuses{
            skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid,
            skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded,
            skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported,
            skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange,
            skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed,
        };

    QCOMPARE(statuses.size(), 5U);
    QVERIFY(skygate::ephemeris::EphemerisEngineQueryStatus::displayName(statuses[0]) == "valid");
    QVERIFY(skygate::ephemeris::EphemerisEngineQueryStatus::displayName(statuses[1]) == "degraded");
    QVERIFY(skygate::ephemeris::EphemerisEngineQueryStatus::displayName(statuses[2]) == "unsupported");
    QVERIFY(skygate::ephemeris::EphemerisEngineQueryStatus::displayName(statuses[3]) == "out of range");
    QVERIFY(skygate::ephemeris::EphemerisEngineQueryStatus::displayName(statuses[4]) == "failed");

    const std::array warningCodes{
        skygate::ephemeris::EphemerisEngineWarning::Code::AccuracyDegraded,
        skygate::ephemeris::EphemerisEngineWarning::Code::UnsupportedBody,
        skygate::ephemeris::EphemerisEngineWarning::Code::DataOutOfRange,
        skygate::ephemeris::EphemerisEngineWarning::Code::ComputationFailed,
    };

    for (const skygate::ephemeris::EphemerisEngineWarning::Code code : warningCodes) {
        const skygate::ephemeris::EphemerisEngineWarning warning(code);
        QVERIFY(!warning.displayText().empty());
        QVERIFY(!skygate::ephemeris::EphemerisEngineWarning::text(code).empty());
    }

    const skygate::ephemeris::EphemerisEngineWarning fallbackTextWarning(
        skygate::ephemeris::EphemerisEngineWarning::Code::DataOutOfRange
    );
    QVERIFY(!fallbackTextWarning.displayText().empty());

    skygate::ephemeris::EphemerisEngineQueryResult metadata;
    QVERIFY(metadata.isSuccessful());
    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(metadata.warningCount(), std::size_t{0});
    QVERIFY(!metadata.hasWarnings());
    QVERIFY(!metadata.effectiveDataValidityRange.has_value());
    QVERIFY(!metadata.estimatedAngularUncertaintyArcsec.has_value());

    skygate::ephemeris::EphemerisDateRange validityRange;
    validityRange.id = "modern";
    validityRange.displayName = "Modern kernel";

    metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
    metadata.addWarning(skygate::ephemeris::EphemerisEngineWarning::Code::ComputationFailed);
    metadata.dataSourceProvenance = "test source";
    metadata.effectiveDataValidityRange = validityRange;
    metadata.appliedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::lightTime();
    metadata.addUnavailableCorrection(skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration());
    metadata.finalizeCorrectionTracking(
        skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
        | skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration()
        | skygate::ephemeris::EphemerisCorrectionFlags::gravitationalLightDeflection()
    );

    QVERIFY(!metadata.isSuccessful());
    QCOMPARE(metadata.warningCount(), std::size_t{2});
    QVERIFY(metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(metadata.effectiveDataValidityRange.has_value());
    QVERIFY(metadata.effectiveDataValidityRange->id == std::string{"modern"});
    QVERIFY(metadata.dataSourceProvenance == std::string{"test source"});
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            metadata.appliedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            metadata.unavailableCorrections, skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            metadata.skippedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::gravitationalLightDeflection()
        )
    );
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
        .horizontal = {
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
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(state.metadata.warningCount(), std::size_t{0});
}

void EphemerisApiModelTests::simpleEngineExposesMetadataDefaults()
{
    auto engineResult = skygate::ephemeris::EphemerisEngineFactory::create();
    QVERIFY(engineResult.isSuccess());
    QVERIFY(engineResult.engine != nullptr);
    const auto& engine = engineResult.engine;

    QCOMPARE(
        static_cast<std::uint8_t>(engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(engine->name() == std::string_view{"Simple ephemeris engine"});

    const auto capabilities = engine->capabilities();
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::solarSystemBodies()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::catalogStars()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::topocentricPositions()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCapabilities::has(
        capabilities, skygate::ephemeris::EphemerisCapabilities::atmosphericRefraction()
    ));
    QVERIFY(!skygate::ephemeris::EphemerisCapabilities::has(
        capabilities, skygate::ephemeris::EphemerisCapabilities::extendedHistoricalRange()
    ));

    const auto options = engine->options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections())
    );
    QVERIFY(options.fallbackToSimpleEngine());
    QVERIFY(!options.enableAtmosphericRefraction());

    const auto dataSetInfo = engine->dataSetInfo();
    QVERIFY(dataSetInfo.id == std::string{"simple"});
    QVERIFY(dataSetInfo.displayName == std::string{"Simple ephemeris engine"});
    QVERIFY(dataSetInfo.version == std::string{"built-in"});
    QVERIFY(!dataSetInfo.provenance.empty());
    QVERIFY(dataSetInfo.dateRanges.empty());
    QVERIFY(engine->supportedDateRanges().empty());
}

static_assert(std::is_enum_v<skygate::ephemeris::EphemerisEngineKind::Type>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisCorrectionFlags::Type>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisEngineQueryStatus::Type>);
static_assert(sizeof(skygate::ephemeris::EphemerisEngineQueryResult) <= 192);

QTEST_MAIN(EphemerisApiModelTests)

#include "EphemerisApiModelTests.moc"
