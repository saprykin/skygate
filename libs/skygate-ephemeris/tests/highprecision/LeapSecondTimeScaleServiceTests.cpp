#include "math/TimeConstants.hpp"
#include "time/CalendarTime.hpp"
#include "engine/DeltaTDataLoader.hpp"
#include "engine/EarthOrientationDataLoader.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/LeapSecondTableLoader.hpp"
#include "engine/LeapSecondTimeScaleService.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace {

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeLeapSecondAsset()
{
    return {
        .id = "leap-seconds",
        .version = "test",
        .provenance = "unit test",
        .content = "#@ version test\n"
                   "#@ source unit test\n"
                   "#@ expires 2027-01-01\n"
                   "effective_utc_date,tai_minus_utc\n"
                   "1972-01-01,10\n"
                   "1972-07-01,11\n"
                   "2015-07-01,36\n"
                   "2017-01-01,37\n",
    };
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::ILeapSecondProvider> makeLeapSecondProvider()
{
    const skygate::ephemeris::LeapSecondTableLoader::Result result =
        skygate::ephemeris::LeapSecondTableLoader::loadFromTextAsset(makeLeapSecondAsset());
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeEarthOrientationAsset()
{
    return {
        .id = "earth-orientation",
        .version = "test-eop",
        .provenance = "unit test",
        .content =
            "#@ version test-eop\n"
            "#@ source unit test\n"
            "#@ expires 2026-07-01\n"
            "#@ prediction_start 2026-05-01\n"
            "#@ prediction_end 2026-06-30\n"
            "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds,predicted\n"
            "2026-04-01,0.10,0.0,0.0,false\n"
            "2026-04-11,0.30,0.0,0.0,false\n"
            "2026-05-01,0.50,0.0,0.0,true\n",
    };
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider>
makeEarthOrientationProvider(const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch = std::nullopt)
{
    skygate::ephemeris::EarthOrientationDataLoader::Options options;
    options.referenceEpoch = referenceEpoch;
    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeEarthOrientationAsset(), options);
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeDeltaTAsset()
{
    return {
        .id = "delta-t",
        .version = "test-delta-t",
        .provenance = "unit test",
        .content = "#@ version test-delta-t\n"
                   "#@ source unit test\n"
                   "#@ expires 2027-01-01\n"
                   "#@ ancient_fallback_start -13200-01-01\n"
                   "#@ ancient_fallback_end 1600-01-01\n"
                   "#@ ancient_fallback_source historical model\n"
                   "#@ ancient_fallback_delta_t_seconds 12000.5\n"
                   "#@ ancient_fallback_uncertainty_seconds 7200\n"
                   "effective_utc_date,delta_t_seconds\n"
                   "1900-01-01,-2.72\n"
                   "2000-01-01,63.83\n"
                   "2026-01-01,69.20\n",
    };
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::IDeltaTProvider> makeDeltaTProvider()
{
    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(makeDeltaTAsset());
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] skygate::core::AstronomicalEpoch makeEpoch(
    const skygate::core::TimeScale timeScale,
    const int year,
    const int month,
    const int day,
    const int hour = 0,
    const int minute = 0,
    const int second = 0,
    const std::uint32_t nanosecond = 0U
)
{
    const auto epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .second = second,
            .nanosecond = nanosecond,
            .timeScale = timeScale,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] skygate::core::AstronomicalEpoch makeUtcEpoch(
    const int year, const int month, const int day, const int hour = 0, const int minute = 0, const int second = 0
)
{
    return makeEpoch(skygate::core::TimeScale::Utc, year, month, day, hour, minute, second);
}

[[nodiscard]] double
secondsBetween(const skygate::core::AstronomicalEpoch& lhs, const skygate::core::AstronomicalEpoch& rhs) noexcept
{
    return ((lhs.julianDatePart1 - rhs.julianDatePart1) + (lhs.julianDatePart2 - rhs.julianDatePart2))
           * skygate::core::TimeConstants::kSecondsPerDay;
}

void compareSecondsBetween(
    const skygate::core::AstronomicalEpoch& lhs,
    const skygate::core::AstronomicalEpoch& rhs,
    const double expectedSeconds
)
{
    QVERIFY(std::abs(secondsBetween(lhs, rhs) - expectedSeconds) < 1.0e-5);
}

}  // namespace

class LeapSecondTimeScaleServiceTests final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsBceCivilDatesWithHistoricalYearHelpers();
    void convertsNormalUtcToTaiAndTt();
    void roundTripsTaiAndTtHelpers();
    void handlesLeapSecondBoundaryOffsets();
    void convertsPositiveLeapSecondCivilLabel();
    void convertsAtTableRangeBoundaries();
    void reportsOutOfRangeWithoutFallback();
    void reportsReverseOutOfRangeWithoutFallback();
    void usesDegradedFallbackForMissingTableWhenAllowed();
    void convertsTtToTdbWithDocumentedApproximation();
    void roundTripsTtAndTdbPreservingPrecision();
    void convertsUtcToTdbThroughTt();
    void convertsUtcToUt1FromExactEopSample();
    void interpolatesUtcToUt1FromEopSamples();
    void convertsUt1ToUtcFromEopSamples();
    void reportsPredictedAndStaleEopWarnings();
    void usesDeltaTFallbackForAncientUt1WhenEopIsOutOfRange();
    void reportsMissingEopWhenUt1FallbackIsDisallowed();
};

void LeapSecondTimeScaleServiceTests::roundTripsBceCivilDatesWithHistoricalYearHelpers()
{
    const std::optional<int> oneBceAstronomicalYear =
        skygate::core::CalendarTime::astronomicalYearFromHistoricalYear(-1);
    QVERIFY(oneBceAstronomicalYear.has_value());
    QCOMPARE(*oneBceAstronomicalYear, 0);
    QCOMPARE(skygate::core::CalendarTime::historicalYearFromAstronomicalYear(*oneBceAstronomicalYear), -1);
    QCOMPARE(skygate::core::CalendarTime::historicalYearFromAstronomicalYear(-1), -2);
    QVERIFY(!skygate::core::CalendarTime::astronomicalYearFromHistoricalYear(0).has_value());

    const skygate::core::AstronomicalEpoch oneBce =
        makeEpoch(skygate::core::TimeScale::Tt, *oneBceAstronomicalYear, 12, 31, 12, 0, 0);
    const std::optional<skygate::core::CivilDateTime> roundTrip =
        skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(oneBce);

    QVERIFY(roundTrip.has_value());
    QCOMPARE(roundTrip->astronomicalYear, 0);
    QCOMPARE(skygate::core::CalendarTime::historicalYearFromAstronomicalYear(roundTrip->astronomicalYear), -1);
    QCOMPARE(roundTrip->month, 12);
    QCOMPARE(roundTrip->day, 31);
    QCOMPARE(roundTrip->hour, 12);
    QCOMPARE(roundTrip->minute, 0);
    QCOMPARE(roundTrip->second, 0);
    QCOMPARE(roundTrip->nanosecond, 0U);
    QCOMPARE(static_cast<std::uint8_t>(roundTrip->timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tt));

    const skygate::core::AstronomicalEpoch firstCeDay = makeEpoch(skygate::core::TimeScale::Tt, 1, 1, 1, 12, 0, 0);
    compareSecondsBetween(firstCeDay, oneBce, 86'400.0);
}

void LeapSecondTimeScaleServiceTests::convertsNormalUtcToTaiAndTt()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(2018, 1, 1);

    const skygate::ephemeris::TimeScaleConversionResult tai = service.convert(utc, skygate::core::TimeScale::Tai);
    QVERIFY(tai.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(tai.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Valid)
    );
    QCOMPARE(static_cast<std::uint8_t>(tai.epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tai));
    compareSecondsBetween(tai.epoch, utc, 37.0);

    const skygate::ephemeris::TimeScaleConversionResult tt = service.convert(utc, skygate::core::TimeScale::Tt);
    QVERIFY(tt.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(tt.epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tt));
    compareSecondsBetween(tt.epoch, utc, 69.184);
}

void LeapSecondTimeScaleServiceTests::roundTripsTaiAndTtHelpers()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::ephemeris::TimeScaleConversionResult tai =
        service.convert(makeUtcEpoch(2018, 1, 1), skygate::core::TimeScale::Tai);
    QVERIFY(tai.isSuccess());

    const skygate::ephemeris::TimeScaleConversionResult tt = service.convert(tai.epoch, skygate::core::TimeScale::Tt);
    QVERIFY(tt.isSuccess());
    compareSecondsBetween(tt.epoch, tai.epoch, 32.184);

    const skygate::ephemeris::TimeScaleConversionResult taiAgain =
        service.convert(tt.epoch, skygate::core::TimeScale::Tai);
    QVERIFY(taiAgain.isSuccess());
    compareSecondsBetween(taiAgain.epoch, tai.epoch, 0.0);

    const skygate::ephemeris::TimeScaleConversionResult utcAgain =
        service.convert(tai.epoch, skygate::core::TimeScale::Utc);
    QVERIFY(utcAgain.isSuccess());
    compareSecondsBetween(utcAgain.epoch, makeUtcEpoch(2018, 1, 1), 0.0);
}

void LeapSecondTimeScaleServiceTests::handlesLeapSecondBoundaryOffsets()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::core::AstronomicalEpoch beforeLeap = makeUtcEpoch(2016, 12, 31, 23, 59, 59);
    const skygate::core::AstronomicalEpoch afterLeap = makeUtcEpoch(2017, 1, 1, 0, 0, 0);

    const skygate::ephemeris::TimeScaleConversionResult beforeTai =
        service.convert(beforeLeap, skygate::core::TimeScale::Tai);
    const skygate::ephemeris::TimeScaleConversionResult afterTai =
        service.convert(afterLeap, skygate::core::TimeScale::Tai);

    QVERIFY(beforeTai.isSuccess());
    QVERIFY(afterTai.isSuccess());
    compareSecondsBetween(beforeTai.epoch, beforeLeap, 36.0);
    compareSecondsBetween(afterTai.epoch, afterLeap, 37.0);
    compareSecondsBetween(afterTai.epoch, beforeTai.epoch, 2.0);
}

void LeapSecondTimeScaleServiceTests::convertsPositiveLeapSecondCivilLabel()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());

    const skygate::core::CivilDateTime leapSecond{
        .astronomicalYear = 2016,
        .month = 12,
        .day = 31,
        .hour = 23,
        .minute = 59,
        .second = 60,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    QVERIFY(skygate::core::CalendarTime::isValidCivilDateTime(leapSecond));
    QVERIFY(!skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(leapSecond).has_value());

    const skygate::ephemeris::TimeScaleConversionResult tai =
        service.convertCivilDateTime(leapSecond, skygate::core::TimeScale::Tai);
    QVERIFY(tai.isSuccess());

    const skygate::ephemeris::TimeScaleConversionResult afterTai =
        service.convert(makeUtcEpoch(2017, 1, 1, 0, 0, 0), skygate::core::TimeScale::Tai);
    compareSecondsBetween(afterTai.epoch, tai.epoch, 1.0);
}

void LeapSecondTimeScaleServiceTests::convertsAtTableRangeBoundaries()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());

    const skygate::ephemeris::TimeScaleConversionResult firstEntry =
        service.convert(makeUtcEpoch(1972, 1, 1), skygate::core::TimeScale::Tai);
    QVERIFY(firstEntry.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(firstEntry.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Valid)
    );

    const skygate::ephemeris::TimeScaleConversionResult expiresAt =
        service.convert(makeUtcEpoch(2027, 1, 1), skygate::core::TimeScale::Tai);
    QVERIFY(expiresAt.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(expiresAt.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Valid)
    );
}

void LeapSecondTimeScaleServiceTests::reportsOutOfRangeWithoutFallback()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::ephemeris::TimeScaleConversionResult result =
        service.convert(makeUtcEpoch(2030, 1, 1), skygate::core::TimeScale::Tai);

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Failed)
    );
    QVERIFY(result.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable));
    QVERIFY(!result.diagnosticText.empty());
}

void LeapSecondTimeScaleServiceTests::reportsReverseOutOfRangeWithoutFallback()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());

    skygate::core::AstronomicalEpoch tai = makeUtcEpoch(2030, 1, 1);
    tai.timeScale = skygate::core::TimeScale::Tai;
    const skygate::ephemeris::TimeScaleConversionResult utcFromTai =
        service.convert(tai, skygate::core::TimeScale::Utc);

    QVERIFY(!utcFromTai.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(utcFromTai.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Failed)
    );
    QVERIFY(utcFromTai.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable));
    QVERIFY(!utcFromTai.diagnosticText.empty());

    skygate::core::AstronomicalEpoch tt = makeUtcEpoch(2030, 1, 1);
    tt.timeScale = skygate::core::TimeScale::Tt;
    const skygate::ephemeris::TimeScaleConversionResult utcFromTt = service.convert(tt, skygate::core::TimeScale::Utc);

    QVERIFY(!utcFromTt.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(utcFromTt.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Failed)
    );
    QVERIFY(utcFromTt.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable));
    QVERIFY(!utcFromTt.diagnosticText.empty());
}

void LeapSecondTimeScaleServiceTests::usesDegradedFallbackForMissingTableWhenAllowed()
{
    skygate::ephemeris::TimeScaleServiceOptions options;
    options.allowDegradedLeapSecondFallback = true;
    options.fallbackTaiMinusUtcSeconds = 42;
    const skygate::ephemeris::LeapSecondTimeScaleService service(nullptr, options);

    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(2026, 1, 1);
    const skygate::ephemeris::TimeScaleConversionResult result = service.convert(utc, skygate::core::TimeScale::Tai);

    QVERIFY(result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Degraded)
    );
    QVERIFY(result.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::LeapSecondTableMissing));
    QVERIFY(result.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::LeapSecondFallbackApplied));
    compareSecondsBetween(result.epoch, utc, 42.0);
}

void LeapSecondTimeScaleServiceTests::convertsTtToTdbWithDocumentedApproximation()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::core::AstronomicalEpoch tt = makeEpoch(skygate::core::TimeScale::Tt, 2000, 1, 1, 12, 0, 0);

    const skygate::ephemeris::TimeScaleConversionResult tdb = service.convert(tt, skygate::core::TimeScale::Tdb);

    QVERIFY(tdb.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(tdb.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Degraded)
    );
    QCOMPARE(static_cast<std::uint8_t>(tdb.epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tdb));
    QVERIFY(tdb.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::TdbApproximationApplied));
    QVERIFY(!skygate::ephemeris::TimeScaleConversionDiagnostics::warningText(
                 skygate::ephemeris::TimeScaleConversionWarningCode::TdbApproximationApplied
    )
                 .empty());
    QVERIFY(std::abs(secondsBetween(tdb.epoch, tt) - -0.00007260319547380129) < 2.0e-3);
}

void LeapSecondTimeScaleServiceTests::roundTripsTtAndTdbPreservingPrecision()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::core::AstronomicalEpoch tt =
        makeEpoch(skygate::core::TimeScale::Tt, 2024, 2, 1, 6, 7, 8, 123'456'789U);

    const skygate::ephemeris::TimeScaleConversionResult tdb = service.convert(tt, skygate::core::TimeScale::Tdb);
    QVERIFY(tdb.isSuccess());

    const skygate::ephemeris::TimeScaleConversionResult ttAgain =
        service.convert(tdb.epoch, skygate::core::TimeScale::Tt);
    QVERIFY(ttAgain.isSuccess());
    QVERIFY(ttAgain.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::TdbApproximationApplied));
    compareSecondsBetween(ttAgain.epoch, tt, 0.0);
}

void LeapSecondTimeScaleServiceTests::convertsUtcToTdbThroughTt()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());
    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(2018, 1, 1);

    const skygate::ephemeris::TimeScaleConversionResult tdb = service.convert(utc, skygate::core::TimeScale::Tdb);

    QVERIFY(tdb.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(tdb.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Degraded)
    );
    QCOMPARE(static_cast<std::uint8_t>(tdb.epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Tdb));
    QVERIFY(tdb.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::TdbApproximationApplied));
    QVERIFY(std::abs(secondsBetween(tdb.epoch, utc) - 69.1839224023) < 2.0e-3);

    const skygate::ephemeris::TimeScaleConversionResult utcAgain =
        service.convert(tdb.epoch, skygate::core::TimeScale::Utc);
    QVERIFY(utcAgain.isSuccess());
    QVERIFY(utcAgain.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::TdbApproximationApplied));
    compareSecondsBetween(utcAgain.epoch, utc, 0.0);
}

void LeapSecondTimeScaleServiceTests::convertsUtcToUt1FromExactEopSample()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(
        makeLeapSecondProvider(), {}, makeEarthOrientationProvider()
    );
    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(2026, 4, 1);

    const skygate::ephemeris::TimeScaleConversionResult ut1 = service.convert(utc, skygate::core::TimeScale::Ut1);

    QVERIFY(ut1.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(ut1.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Valid)
    );
    QCOMPARE(static_cast<std::uint8_t>(ut1.epoch.timeScale), static_cast<std::uint8_t>(skygate::core::TimeScale::Ut1));
    compareSecondsBetween(ut1.epoch, utc, 0.10);
}

void LeapSecondTimeScaleServiceTests::interpolatesUtcToUt1FromEopSamples()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(
        makeLeapSecondProvider(), {}, makeEarthOrientationProvider()
    );
    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(2026, 4, 6);

    const skygate::ephemeris::TimeScaleConversionResult ut1 = service.convert(utc, skygate::core::TimeScale::Ut1);

    QVERIFY(ut1.isSuccess());
    compareSecondsBetween(ut1.epoch, utc, 0.20);
}

void LeapSecondTimeScaleServiceTests::convertsUt1ToUtcFromEopSamples()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(
        makeLeapSecondProvider(), {}, makeEarthOrientationProvider()
    );
    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(2026, 4, 6);
    const skygate::ephemeris::TimeScaleConversionResult ut1 = service.convert(utc, skygate::core::TimeScale::Ut1);
    QVERIFY(ut1.isSuccess());

    const skygate::ephemeris::TimeScaleConversionResult utcAgain =
        service.convert(ut1.epoch, skygate::core::TimeScale::Utc);

    QVERIFY(utcAgain.isSuccess());
    compareSecondsBetween(utcAgain.epoch, utc, 0.0);
}

void LeapSecondTimeScaleServiceTests::reportsPredictedAndStaleEopWarnings()
{
    const skygate::ephemeris::LeapSecondTimeScaleService predictedService(
        makeLeapSecondProvider(), {}, makeEarthOrientationProvider()
    );
    const skygate::ephemeris::TimeScaleConversionResult predicted =
        predictedService.convert(makeUtcEpoch(2026, 5, 1), skygate::core::TimeScale::Ut1);

    QVERIFY(predicted.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(predicted.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Degraded)
    );
    QVERIFY(predicted.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EarthOrientationDataPredicted));

    const skygate::ephemeris::LeapSecondTimeScaleService staleService(
        makeLeapSecondProvider(), {}, makeEarthOrientationProvider(makeUtcEpoch(2026, 8, 1))
    );
    const skygate::ephemeris::TimeScaleConversionResult stale =
        staleService.convert(makeUtcEpoch(2026, 4, 1), skygate::core::TimeScale::Ut1);

    QVERIFY(stale.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(stale.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Degraded)
    );
    QVERIFY(stale.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EarthOrientationDataStale));
}

void LeapSecondTimeScaleServiceTests::usesDeltaTFallbackForAncientUt1WhenEopIsOutOfRange()
{
    skygate::ephemeris::TimeScaleServiceOptions options;
    options.allowDegradedLeapSecondFallback = true;
    options.fallbackTaiMinusUtcSeconds = 0;
    options.allowUt1DeltaTFallback = true;
    const skygate::ephemeris::LeapSecondTimeScaleService service(
        makeLeapSecondProvider(), options, makeEarthOrientationProvider(), makeDeltaTProvider()
    );
    const skygate::core::AstronomicalEpoch utc = makeUtcEpoch(-5000, 1, 1);

    const skygate::ephemeris::TimeScaleConversionResult ut1 = service.convert(utc, skygate::core::TimeScale::Ut1);

    QVERIFY(ut1.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(ut1.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Degraded)
    );
    QVERIFY(ut1.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData));
    QVERIFY(ut1.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::DeltaTFallbackApplied));
    compareSecondsBetween(ut1.epoch, utc, -11968.316);
}

void LeapSecondTimeScaleServiceTests::reportsMissingEopWhenUt1FallbackIsDisallowed()
{
    const skygate::ephemeris::LeapSecondTimeScaleService service(makeLeapSecondProvider());

    const skygate::ephemeris::TimeScaleConversionResult result =
        service.convert(makeUtcEpoch(2026, 4, 1), skygate::core::TimeScale::Ut1);

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScaleConversionStatus::Failed)
    );
    QVERIFY(result.hasWarning(skygate::ephemeris::TimeScaleConversionWarningCode::EarthOrientationDataMissing));
    QVERIFY(!result.diagnosticText.empty());
}

QTEST_MAIN(LeapSecondTimeScaleServiceTests)

#include "LeapSecondTimeScaleServiceTests.moc"
