#include "EquatorialCoordinate.hpp"
#include "EphemerisRequest.hpp"
#include "HighPrecisionEngineFixture.hpp"
#include "PassThroughApparentPlaceCalculator.hpp"
#include "engine/DeltaTDataLoader.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/EphemerisTextDataAsset.hpp"
#include "engine/IDeltaTProvider.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/ILeapSecondProvider.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/LeapSecondTableLoader.hpp"
#include "engine/LeapSecondTimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/TimeScaleConversionStatus.hpp"
#include "engine/TimeScaleConversionWarningCode.hpp"
#include "engine/TimeScaleServiceOptions.hpp"
#include "math/Vector3d.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/CivilDateTime.hpp"
#include "time/TimeScale.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/CelestialFrameTransformRequest.hpp"
#include "engine/highprecision/CelestialFrameTransformResult.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using skygate::ephemeris::tests::HighPrecisionEngineFixture;
using skygate::ephemeris::tests::PassThroughApparentPlaceCalculator;

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

[[nodiscard]] skygate::core::AstronomicalEpoch makeEpoch(
    const skygate::core::TimeScale timeScale,
    const int year,
    const int month,
    const int day,
    const int hour = 0,
    const int minute = 0,
    const int second = 0
)
{
    const std::optional<skygate::core::AstronomicalEpoch> epoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
            skygate::core::CivilDateTime{
                .astronomicalYear = year,
                .month = month,
                .day = day,
                .hour = hour,
                .minute = minute,
                .second = second,
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

[[nodiscard]] EphemerisTextDataAsset makeLeapSecondAsset()
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

[[nodiscard]] std::shared_ptr<const ILeapSecondProvider>
makeLeapSecondProvider(const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch = std::nullopt)
{
    LeapSecondTableLoader::Options options;
    options.referenceEpoch = referenceEpoch;
    const LeapSecondTableLoader::Result result =
        LeapSecondTableLoader::loadFromTextAsset(makeLeapSecondAsset(), options);
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] EphemerisTextDataAsset makeDeltaTAsset()
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

[[nodiscard]] std::shared_ptr<const IDeltaTProvider> makeDeltaTProvider()
{
    const DeltaTDataLoader::Result result = DeltaTDataLoader::loadFromTextAsset(makeDeltaTAsset());
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

class StaticSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    explicit StaticSolarSystemCalculator(HighPrecisionCalculatorResult result) : m_result(std::move(result)) {}

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        return m_result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

private:
    HighPrecisionCalculatorResult m_result;
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
};

void mergeConversionMetadata(EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion) noexcept
{
    if (conversion.status == TimeScaleConversionStatus::Type::Failed) {
        metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return;
    }
    if (conversion.status == TimeScaleConversionStatus::Type::Degraded
        && metadata.status == skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid) {
        metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded;
        metadata.addWarning(EphemerisEngineWarning::Code::AccuracyDegraded);
    }
    if (conversion.hasWarning(TimeScaleConversionWarningCode::Type::LeapSecondTableMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::Type::DeltaTUnavailable)) {
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    }
    if (conversion.hasWarning(TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable)
        || conversion.hasWarning(TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData)) {
        metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    }
}

class MetadataFrameTransformer final : public IFrameTransformer {
public:
    explicit MetadataFrameTransformer(std::shared_ptr<const ITimeScaleService> timeScaleService)
        : m_timeScaleService(std::move(timeScaleService))
    {
    }

    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transform(const CelestialFrameTransformRequest& request) const override
    {
        const bool usesTerrestrialFrame = CelestialReferenceFrame::isTerrestrial(request.sourceFrame)
                                          || CelestialReferenceFrame::isTerrestrial(request.targetFrame);

        std::vector<CelestialFrameTransformResult> results;
        results.reserve(request.vectors.size());
        for (const Vector3d& vector : request.vectors) {
            CelestialFrameTransformResult result;
            result.vector = vector;
            result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
            result.metadata.dataSourceProvenance = "metadata frame transformer";
            result.metadata.appliedCorrections = usesTerrestrialFrame ? EphemerisCorrectionFlags::earthOrientation()
                                                                      : EphemerisCorrectionFlags::precessionNutation();
            if (m_timeScaleService == nullptr) {
                result.vector.reset();
                result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
                result.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
                results.push_back(std::move(result));
                continue;
            }

            const TimeScaleConversionResult conversion = m_timeScaleService->convert(
                request.epoch, usesTerrestrialFrame ? skygate::core::TimeScale::Ut1 : skygate::core::TimeScale::Tt
            );
            mergeConversionMetadata(result.metadata, conversion);
            if (!conversion.isSuccess()) {
                result.vector.reset();
            }
            results.push_back(std::move(result));
        }
        return results;
    }

private:
    std::shared_ptr<const ITimeScaleService> m_timeScaleService;
};

[[nodiscard]] HighPrecisionCalculatorResult makeApparentPipelineInputResult()
{
    HighPrecisionCalculatorResult result;
    result.equatorial = EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 20.0,
    };
    result.observerRelativePositionAu = Vector3d{.x = 0.75, .y = 0.25, .z = 0.5};
    result.metadata.dataSourceProvenance = "fixture geometric solar-system state";
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::geometric();
    return result;
}

[[nodiscard]] HighPrecisionEphemerisEngine::Dependencies makeApparentPipelineDependencies(
    std::shared_ptr<ISolarSystemStateCalculator> solarSystemCalculator,
    std::shared_ptr<const ITimeScaleService> timeScaleService,
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProviderForFrame,
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProviderForApparent
)
{
    static_cast<void>(earthOrientationProviderForFrame);
    auto frameTransformer = std::make_shared<MetadataFrameTransformer>(timeScaleService);
    auto apparentPlaceCalculator = std::make_shared<ApparentPlaceCalculator>(
        frameTransformer, timeScaleService, std::move(earthOrientationProviderForApparent)
    );

    HighPrecisionEphemerisEngine::Dependencies dependencies =
        HighPrecisionEngineFixture::dependencies(std::move(solarSystemCalculator), {}, apparentPlaceCalculator);
    dependencies.timeScaleService = std::move(timeScaleService);
    dependencies.earthOrientationProvider = std::move(earthOrientationProviderForFrame);
    dependencies.frameTransformer = std::move(frameTransformer);
    return dependencies;
}

}  // namespace

class HighPrecisionResultBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultResultBuilderAssemblesValidMetadata();
    void defaultResultBuilderTracksCorrectionOutcomes();
    void defaultResultBuilderDegradesOutOfRangeFallback();
    void defaultResultBuilderKeepsOutOfRangeNoFallback();
    void defaultResultBuilderKeepsFailedWithoutFallback();
    void defaultResultBuilderKeepsDegradedDataWarnings();
    void defaultResultBuilderAssemblesLongRangeFallback();
    void defaultResultBuilderStaleOrientationWarnings();
    void defaultResultBuilderStaleLeapSecondWarnings();
    void defaultResultBuilderAncientDeltaTWarnings();
};

void HighPrecisionResultBuilderTests::defaultResultBuilderAssemblesValidMetadata()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 5.0,
    };
    calculatorResult.horizontal = HorizontalCoordinate{
        .altitudeDeg = 35.0,
        .azimuthDeg = 180.0,
    };
    calculatorResult.metadata.dataSourceProvenance = "unit test kernel";
    calculatorResult.metadata.effectiveDataValidityRange = EphemerisDateRange{
        .id = "modern",
        .displayName = "Modern range",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
    };
    calculatorResult.metadata.estimatedAngularUncertaintyArcsec = 0.12;
    calculatorResult.metadata.appliedCorrections = EphemerisCorrectionFlags::lightTime();

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create()
        )
    );

    const auto state = engine.computeBodyState(HighPrecisionEngineFixture::request(), "sun");

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QVERIFY(solarSystemCalculator->lastBodyId() == std::string{"sun"});
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(state->equatorial.rightAscensionHours, 4.0);
    QCOMPARE(state->equatorial.declinationDeg, 5.0);
    QCOMPARE(state->horizontal.altitudeDeg, 35.0);
    QCOMPARE(state->horizontal.azimuthDeg, 180.0);
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"unit test kernel"});
    QVERIFY(state->metadata.effectiveDataValidityRange.has_value());
    QVERIFY(state->metadata.effectiveDataValidityRange->id == std::string{"modern"});
    QVERIFY(state->metadata.estimatedAngularUncertaintyArcsec.has_value());
    QCOMPARE(*state->metadata.estimatedAngularUncertaintyArcsec, 0.12);
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
}

void HighPrecisionResultBuilderTests::defaultResultBuilderTracksCorrectionOutcomes()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 5.0,
    };
    calculatorResult.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded;
    calculatorResult.metadata.appliedCorrections = EphemerisCorrectionFlags::lightTime();
    calculatorResult.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::stellarAberration());

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create()
        )
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.options.setCorrectionFlags(
        EphemerisCorrectionFlags::lightTime() | EphemerisCorrectionFlags::stellarAberration()
        | EphemerisCorrectionFlags::gravitationalLightDeflection()
    );

    const auto state = engine.computeBodyState(request, "sun");

    QVERIFY(state.has_value());
    QCOMPARE(state->metadata.requestedCorrections, request.options.correctionFlags());
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        state->metadata.appliedCorrections, EphemerisCorrectionFlags::stellarAberration()
    ));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.unavailableCorrections, EphemerisCorrectionFlags::stellarAberration()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        state->metadata.unavailableCorrections, EphemerisCorrectionFlags::lightTime()
    ));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.skippedCorrections, EphemerisCorrectionFlags::gravitationalLightDeflection()
        )
    );
    QVERIFY(!skygate::ephemeris::EphemerisCorrectionFlags::has(
        state->metadata.skippedCorrections, EphemerisCorrectionFlags::lightTime()
    ));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
}

void HighPrecisionResultBuilderTests::defaultResultBuilderDegradesOutOfRangeFallback()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = EquatorialCoordinate{
        .rightAscensionHours = 6.0,
        .declinationDeg = -7.0,
    };
    calculatorResult.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    calculatorResult.metadata.dataSourceProvenance = "missing DE441 fallback";

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create()
        )
    );

    const auto state = engine.computeBodyState(HighPrecisionEngineFixture::request(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"missing DE441 fallback"});
    QCOMPARE(state->equatorial.rightAscensionHours, 6.0);
    QCOMPARE(state->equatorial.declinationDeg, -7.0);
}

void HighPrecisionResultBuilderTests::defaultResultBuilderKeepsOutOfRangeNoFallback()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    calculatorResult.metadata.dataSourceProvenance = "kernel out of range";

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.options.setFallbackToSimpleEngine(false);

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        request.options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create()
        )
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::OutOfRange)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"kernel out of range"});
    QVERIFY(std::isnan(state->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(state->equatorial.declinationDeg));
}

void HighPrecisionResultBuilderTests::defaultResultBuilderKeepsFailedWithoutFallback()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    calculatorResult.metadata.dataSourceProvenance = "missing kernel";

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create()
        )
    );

    const auto state = engine.computeBodyState(HighPrecisionEngineFixture::request(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"missing kernel"});
    QVERIFY(std::isnan(state->equatorial.rightAscensionHours));
}

void HighPrecisionResultBuilderTests::defaultResultBuilderKeepsDegradedDataWarnings()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = EquatorialCoordinate{
        .rightAscensionHours = 8.0,
        .declinationDeg = 9.0,
    };
    calculatorResult.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded;
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::AccuracyDegraded);
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    calculatorResult.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
    calculatorResult.metadata.dataSourceProvenance = "stale EOP, stale leap-second, Delta T fallback";

    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, {}, PassThroughApparentPlaceCalculator::create()
        )
    );

    const auto state = engine.computeBodyState(HighPrecisionEngineFixture::request(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"stale EOP, stale leap-second, Delta T fallback"});
    QCOMPARE(state->equatorial.rightAscensionHours, 8.0);
    QCOMPARE(state->equatorial.declinationDeg, 9.0);
}

void HighPrecisionResultBuilderTests::defaultResultBuilderAssemblesLongRangeFallback()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto kernel = HighPrecisionEngineFixture::longRangeFallbackKernel();
    auto solarSystemCalculator = std::make_shared<SolarSystemStateCalculator>(kernel);
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(solarSystemCalculator)
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch = makeEpoch(skygate::core::TimeScale::Tdb, -5000, 1, 1);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::geometric());

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(kernel->callCount(), 1);
    QCOMPARE(kernel->lastTargetNaifId(), 10);
    QCOMPARE(kernel->lastCenterNaifId(), 399);
    QCOMPARE(
        static_cast<std::uint8_t>(kernel->lastEpoch().timeScale),
        static_cast<std::uint8_t>(skygate::core::TimeScale::Tdb)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::MissingEphemerisData));
    QVERIFY(
        state->metadata.dataSourceProvenance == std::string{"missing DE441 long-range kernel; modern kernel fallback"}
    );
    QVERIFY(state->metadata.effectiveDataValidityRange.has_value());
    QVERIFY(state->metadata.effectiveDataValidityRange->id == std::string{"de440-modern"});
    QVERIFY(state->metadata.estimatedAngularUncertaintyArcsec.has_value());
    QCOMPARE(*state->metadata.estimatedAngularUncertaintyArcsec, 3600.0);
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
}

void HighPrecisionResultBuilderTests::defaultResultBuilderStaleOrientationWarnings()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(makeApparentPipelineInputResult());
    const std::shared_ptr<const IEarthOrientationProvider> staleEarthOrientationProvider =
        HighPrecisionEngineFixture::earthOrientationProvider(makeUtcEpoch(2026, 8, 1));
    const auto timeScaleService = std::make_shared<LeapSecondTimeScaleService>(
        makeLeapSecondProvider(), TimeScaleServiceOptions{}, staleEarthOrientationProvider
    );
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        makeApparentPipelineDependencies(
            solarSystemCalculator, timeScaleService, staleEarthOrientationProvider, staleEarthOrientationProvider
        )
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch = makeUtcEpoch(2026, 4, 1);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::topocentric());
    request.options.setEnableAtmosphericRefraction(false);

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"fixture geometric solar-system state"});
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
    QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(state->horizontal.azimuthDeg));
}

void HighPrecisionResultBuilderTests::defaultResultBuilderStaleLeapSecondWarnings()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(makeApparentPipelineInputResult());
    const auto timeScaleService =
        std::make_shared<LeapSecondTimeScaleService>(makeLeapSecondProvider(makeUtcEpoch(2027, 2, 1)));
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        makeApparentPipelineDependencies(solarSystemCalculator, timeScaleService, nullptr, nullptr)
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch = makeUtcEpoch(2026, 4, 1);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::apparent());
    request.options.setEnableAtmosphericRefraction(false);

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
}

void HighPrecisionResultBuilderTests::defaultResultBuilderAncientDeltaTWarnings()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(makeApparentPipelineInputResult());
    const std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider =
        HighPrecisionEngineFixture::earthOrientationProvider();
    TimeScaleServiceOptions options;
    options.allowDegradedLeapSecondFallback = true;
    options.fallbackTaiMinusUtcSeconds = 0;
    options.allowUt1DeltaTFallback = true;
    options.earthOrientationSampleOptions.allowOutOfRangeNearestSampleFallback = false;
    options.earthOrientationSampleOptions.allowMissingDataZeroFallback = false;
    const auto timeScaleService = std::make_shared<LeapSecondTimeScaleService>(
        makeLeapSecondProvider(), options, earthOrientationProvider, makeDeltaTProvider()
    );
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        makeApparentPipelineDependencies(solarSystemCalculator, timeScaleService, nullptr, nullptr)
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch = makeUtcEpoch(-5000, 1, 1);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::topocentric());
    request.options.setEnableAtmosphericRefraction(false);

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            state->metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
        )
    );
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
    QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(state->horizontal.azimuthDeg));
}

QTEST_APPLESS_MAIN(HighPrecisionResultBuilderTests)

#include "HighPrecisionResultBuilderTests.moc"
