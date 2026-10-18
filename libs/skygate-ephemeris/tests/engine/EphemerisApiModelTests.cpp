#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "ObservationEventStatus.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "UtcTimeCodec.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/CelestialFrameTransformResult.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

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

}  // namespace

class EphemerisApiModelTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactlyTwoEngineKinds();
    void constructsHighPrecisionModelDefaults();
    void combinesCorrectionFlags();
    void constructsRequestAndDataSetModels();
    void constructsCatalogStarAstrometryModel();
    void constructsAndNormalizesAstronomicalTimes();
    void exposesCelestialReferenceFrameHelpers();
    void constructsCelestialFrameTransformResults();
    void convertsCivilDatesAndDefinesNoYearZeroPolicy();
    void constructsResultStatusAndWarningModels();
    void engineWarningTextMatchesTimeScaleWarningTexts();
    void bodyAndEventStatusLabelsAreCanonical();
    void keepsLegacyBodyStateFieldsReadableWithMetadata();
    void simpleEngineExposesMetadataDefaults();
    void distinguishesGeometricFromNoCorrections();
    void rejectsNonFiniteAtmosphericNumericOptions();
    void isIdenticalToDistinguishesSignedZeroOptions();
    void unavailableCorrectionDegradesValidResult();
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
    skygate::core::AstronomicalEpoch epoch;
    epoch.julianDatePart1 = 2'460'310.0;
    epoch.julianDatePart2 = 0.5;
    epoch.timeScale = skygate::core::TimeScale::Tt;

    skygate::ephemeris::EphemerisRequest request;
    request.epoch = epoch;
    request.context.observer.latitudeDeg = 37.7749;
    request.context.observer.longitudeDeg = -122.4194;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);

    QCOMPARE(
        static_cast<std::uint8_t>(request.epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tt)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(skygate::core::UtcTimeCodec::toEpochMicros(request.context.utcTime), 1'704'067'200'000'000LL);

    skygate::ephemeris::EphemerisDateRange modernRange;
    modernRange.id = "modern";
    modernRange.displayName = "Modern kernel";
    modernRange.start = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb
    };
    modernRange.end = skygate::core::AstronomicalEpoch{
        .julianDatePart1 = 2'600'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb
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
        static_cast<std::uint8_t>(skygate::core::TimeScale::Tdb)
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
                .timeScale = skygate::core::TimeScale::Tt,
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

void EphemerisApiModelTests::constructsAndNormalizesAstronomicalTimes()
{
    const skygate::core::AstronomicalEpoch overflowEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 1.75,
        .timeScale = skygate::core::TimeScale::Tt,
    };
    const auto normalizedOverflow = overflowEpoch.normalized();
    QCOMPARE(normalizedOverflow.julianDatePart1, 2'451'546.0);
    QCOMPARE(normalizedOverflow.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(normalizedOverflow.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tt)
    );

    const skygate::core::AstronomicalEpoch negativeFractionEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = -0.25,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    const auto normalizedNegativeFraction = negativeFractionEpoch.normalized();
    QCOMPARE(normalizedNegativeFraction.julianDatePart1, 2'451'544.0);
    QCOMPARE(normalizedNegativeFraction.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(normalizedNegativeFraction.timeScale),
        static_cast<std::uint8_t>(skygate::core::TimeScale::Tdb)
    );

    const skygate::core::AstronomicalEpoch utcEpoch{
        .julianDatePart1 = 2'451'545.0,
        .julianDatePart2 = 0.25,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    const auto plusSeconds = utcEpoch.addSeconds(43'200.0);
    QCOMPARE(plusSeconds.julianDatePart1, 2'451'545.0);
    QCOMPARE(plusSeconds.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(plusSeconds.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Utc)
    );

    const auto plusMinutesInTai = utcEpoch.addMinutes(720.0, skygate::core::TimeScale::Tai);
    QCOMPARE(plusMinutesInTai.julianDatePart1, 2'451'545.0);
    QCOMPARE(plusMinutesInTai.julianDatePart2, 0.75);
    QCOMPARE(
        static_cast<std::uint8_t>(plusMinutesInTai.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tai)
    );

    const skygate::core::CivilDateTime j2000Noon{
        .astronomicalYear = 2000,
        .month = 1,
        .day = 1,
        .hour = 12,
        .minute = 0,
        .second = 0,
        .timeScale = skygate::core::TimeScale::Tt,
    };
    const auto j2000Epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(j2000Noon);
    QVERIFY(j2000Epoch.has_value());
    QCOMPARE(j2000Epoch->julianDatePart1, 2'451'545.0);
    QCOMPARE(j2000Epoch->julianDatePart2, 0.0);
    QCOMPARE(static_cast<std::uint8_t>(j2000Epoch->timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tt));
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
    const skygate::core::CivilDateTime preciseDateTime{
        .astronomicalYear = 2026,
        .month = 5,
        .day = 13,
        .hour = 12,
        .minute = 34,
        .second = 56,
        .nanosecond = 123'456'789U,
        .timeScale = skygate::core::TimeScale::Utc,
    };

    const auto preciseEpoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(preciseDateTime);
    QVERIFY(preciseEpoch.has_value());
    const auto preciseRoundTrip = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(*preciseEpoch);
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

    const skygate::core::CivilDateTime astronomicalYearZero{
        .astronomicalYear = 0,
        .month = 1,
        .day = 1,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    const auto yearZeroEpoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(astronomicalYearZero);
    QVERIFY(yearZeroEpoch.has_value());
    const auto yearZeroRoundTrip = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(*yearZeroEpoch);
    QVERIFY(yearZeroRoundTrip.has_value());
    QCOMPARE(yearZeroRoundTrip->astronomicalYear, 0);
    QCOMPARE(yearZeroRoundTrip->month, 1);
    QCOMPARE(yearZeroRoundTrip->day, 1);

    const auto astronomicalFromOneBce = skygate::core::CalendarTime::astronomicalYearFromHistoricalYear(-1);
    QVERIFY(astronomicalFromOneBce.has_value());
    QCOMPARE(*astronomicalFromOneBce, 0);
    const auto astronomicalFromFortyFourBce = skygate::core::CalendarTime::astronomicalYearFromHistoricalYear(-44);
    QVERIFY(astronomicalFromFortyFourBce.has_value());
    QCOMPARE(*astronomicalFromFortyFourBce, -43);
    QVERIFY(!skygate::core::CalendarTime::astronomicalYearFromHistoricalYear(0).has_value());
    QCOMPARE(skygate::core::CalendarTime::historicalYearFromAstronomicalYear(0), -1);
    QCOMPARE(skygate::core::CalendarTime::historicalYearFromAstronomicalYear(-43), -44);
    QCOMPARE(skygate::core::CalendarTime::historicalYearFromAstronomicalYear(2026), 2026);

    const skygate::core::CivilDateTime invalidLeapDay{
        .astronomicalYear = 2023,
        .month = 2,
        .day = 29,
    };
    QVERIFY(!skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(invalidLeapDay).has_value());

    const skygate::core::CivilDateTime leapSecondLabel{
        .astronomicalYear = 2016,
        .month = 12,
        .day = 31,
        .hour = 23,
        .minute = 59,
        .second = 60,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    QVERIFY(skygate::core::CalendarTime::isValidCivilDateTime(leapSecondLabel));
    QVERIFY(!skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(leapSecondLabel).has_value());
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

void EphemerisApiModelTests::engineWarningTextMatchesTimeScaleWarningTexts()
{
    struct SharedWarning {
        skygate::ephemeris::EphemerisEngineWarning::Code engineCode;
        skygate::ephemeris::TimeScaleConversionWarningCode::Type timeScaleCode;
    };

    constexpr std::array sharedWarnings{
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::LeapSecondTableMissing,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::LeapSecondTableMissing
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::LeapSecondTableStale,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::LeapSecondTableStale
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EpochOutsideLeapSecondTable,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::LeapSecondFallbackApplied,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::UnsupportedConversion
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::InvalidTimeScaleInput,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::InvalidInput
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::TdbApproximationApplied,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::TdbApproximationApplied
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationDataMissing,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationDataStale,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataStale
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationDataPredicted,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EpochOutsideEarthOrientationData,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::DeltaTFallbackApplied,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::DeltaTUnavailable,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::DeltaTUnavailable
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationDataEstimated,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated
        },
    };

    for (const SharedWarning& sharedWarning : sharedWarnings) {
        QCOMPARE(
            skygate::ephemeris::EphemerisEngineWarning::text(sharedWarning.engineCode),
            skygate::ephemeris::TimeScaleConversionDiagnostics::warningText(sharedWarning.timeScaleCode)
        );
    }

    constexpr std::array aliasWarnings{
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationStaleData,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataStale
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationPredictedData,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationMissingData,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationEpochOutsideRange,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData
        },
        SharedWarning{
            skygate::ephemeris::EphemerisEngineWarning::Code::EarthOrientationEstimatedData,
            skygate::ephemeris::TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated
        },
    };

    for (const SharedWarning& aliasWarning : aliasWarnings) {
        QCOMPARE(
            skygate::ephemeris::EphemerisEngineWarning::text(aliasWarning.engineCode),
            skygate::ephemeris::TimeScaleConversionDiagnostics::warningText(aliasWarning.timeScaleCode)
        );
    }
}

void EphemerisApiModelTests::bodyAndEventStatusLabelsAreCanonical()
{
    using skygate::ephemeris::BaseCelestialBody;
    using skygate::ephemeris::DeepSkyObjectInfo;
    using skygate::ephemeris::ObservationEventStatus;

    constexpr std::array bodyKinds{
        BaseCelestialBody::Kind::Star,
        BaseCelestialBody::Kind::Planet,
        BaseCelestialBody::Kind::Moon,
        BaseCelestialBody::Kind::Sun,
        BaseCelestialBody::Kind::Constellation,
        BaseCelestialBody::Kind::DeepSkyObject,
    };

    for (const BaseCelestialBody::Kind kind : bodyKinds) {
        QVERIFY(!BaseCelestialBody::kindLabel(kind).empty());
    }
    QCOMPARE(BaseCelestialBody::kindLabel(BaseCelestialBody::Kind::DeepSkyObject), std::string_view("Deep sky object"));

    constexpr std::array deepSkyKinds{
        DeepSkyObjectInfo::Kind::Unknown,
        DeepSkyObjectInfo::Kind::Galaxy,
        DeepSkyObjectInfo::Kind::OpenCluster,
        DeepSkyObjectInfo::Kind::GlobularCluster,
        DeepSkyObjectInfo::Kind::Nebula,
        DeepSkyObjectInfo::Kind::PlanetaryNebula,
        DeepSkyObjectInfo::Kind::Asterism,
    };

    for (const DeepSkyObjectInfo::Kind kind : deepSkyKinds) {
        QVERIFY(!DeepSkyObjectInfo::kindLabel(kind).empty());
    }
    QCOMPARE(DeepSkyObjectInfo::kindLabel(DeepSkyObjectInfo::Kind::Unknown), std::string_view("Deep sky object"));

    constexpr std::array eventStatuses{
        ObservationEventStatus::Type::Available,
        ObservationEventStatus::Type::AlwaysAbove,
        ObservationEventStatus::Type::AlwaysBelow,
        ObservationEventStatus::Type::NoEventInSearchWindow,
        ObservationEventStatus::Type::InvalidInput,
        ObservationEventStatus::Type::Unresolved,
    };

    for (const ObservationEventStatus::Type status : eventStatuses) {
        QVERIFY(!ObservationEventStatus::displayText(status).empty());
    }
    QCOMPARE(
        ObservationEventStatus::displayText(ObservationEventStatus::Type::AlwaysAbove), std::string_view("Always up")
    );
    QCOMPARE(
        ObservationEventStatus::displayText(ObservationEventStatus::Type::AlwaysBelow), std::string_view("Always down")
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

void EphemerisApiModelTests::distinguishesGeometricFromNoCorrections()
{
    const auto geometric = skygate::ephemeris::EphemerisCorrectionFlags::geometric();
    const auto noCorrections = skygate::ephemeris::EphemerisCorrectionFlags::noCorrections();

    QVERIFY(geometric != noCorrections);
    QCOMPARE(
        static_cast<std::uint32_t>(noCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections())
    );
    QVERIFY(!geometric.hasCorrections());
    QVERIFY(!noCorrections.hasCorrections());
    QVERIFY(skygate::ephemeris::EphemerisCorrectionFlags::lightTime().hasCorrections());
}

void EphemerisApiModelTests::rejectsNonFiniteAtmosphericNumericOptions()
{
    skygate::ephemeris::EphemerisEngineOptions options;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    options.setAtmosphericPressureHpa(nan);
    QCOMPARE(options.atmosphericPressureHpa(), 1013.25);
    QVERIFY(!options.isOptionSet(skygate::ephemeris::EphemerisEngineOptions::Key::AtmosphericPressureHpa));

    options.setRelativeHumidity(nan);
    QCOMPARE(options.relativeHumidity(), 0.0);
    QVERIFY(!options.isOptionSet(skygate::ephemeris::EphemerisEngineOptions::Key::RelativeHumidity));

    options.setAtmosphericPressureHpa(875.0);
    QCOMPARE(options.atmosphericPressureHpa(), 875.0);
    QVERIFY(options.isOptionSet(skygate::ephemeris::EphemerisEngineOptions::Key::AtmosphericPressureHpa));
}

void EphemerisApiModelTests::isIdenticalToDistinguishesSignedZeroOptions()
{
    skygate::ephemeris::EphemerisEngineOptions positiveZeroOptions;
    positiveZeroOptions.setAtmosphericTemperatureC(0.0);
    skygate::ephemeris::EphemerisEngineOptions negativeZeroOptions = positiveZeroOptions;
    negativeZeroOptions.setAtmosphericTemperatureC(-0.0);

    QVERIFY(positiveZeroOptions == negativeZeroOptions);
    QVERIFY(!positiveZeroOptions.isIdenticalTo(negativeZeroOptions));
    QVERIFY(!negativeZeroOptions.isIdenticalTo(positiveZeroOptions));
    QVERIFY(positiveZeroOptions.isIdenticalTo(positiveZeroOptions));

    skygate::ephemeris::EphemerisEngineOptions warmerOptions = positiveZeroOptions;
    warmerOptions.setAtmosphericTemperatureC(0.5);
    QVERIFY(!positiveZeroOptions.isIdenticalTo(warmerOptions));
}

void EphemerisApiModelTests::unavailableCorrectionDegradesValidResult()
{
    skygate::ephemeris::EphemerisEngineQueryResult metadata;
    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );

    metadata.addUnavailableCorrection(skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction());

    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            metadata.unavailableCorrections, skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
        )
    );
}

static_assert(std::is_enum_v<skygate::ephemeris::EphemerisEngineKind::Type>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisCorrectionFlags::Type>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisEngineQueryStatus::Type>);
// Guards the ABI/stack-usage ceiling for the QML-facing result type so it stays
// cheap to pass by value across the UI boundary.
static_assert(sizeof(skygate::ephemeris::EphemerisEngineQueryResult) <= 224);

QTEST_MAIN(EphemerisApiModelTests)

#include "EphemerisApiModelTests.moc"
