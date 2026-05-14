#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/FrameTransformer.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"
#include "skygate/ephemeris/DeltaTProvider.hpp"
#include "skygate/ephemeris/EarthOrientationProvider.hpp"
#include "skygate/ephemeris/LeapSecondProvider.hpp"
#include "skygate/ephemeris/TimeScaleService.hpp"

#include <QtTest/QtTest>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
namespace core = skygate::core;

[[nodiscard]] CelestialBody makeSunBody()
{
    return {
        .id = "sun",
        .displayName = "Sun",
        .type = CelestialBodyType::Sun,
        .ephemerisSource = CelestialBodyEphemerisSource::Sun,
    };
}

[[nodiscard]] CelestialBody makeStarBody()
{
    return {
        .id = "vega",
        .displayName = "Vega",
        .type = CelestialBodyType::Star,
        .ephemerisSource = CelestialBodyEphemerisSource::Star,
    };
}

[[nodiscard]] CelestialBody
makeFixedStarBody(std::string id, const double rightAscensionHours, const double declinationDeg)
{
    CelestialBody body;
    body.id = std::move(id);
    body.displayName = body.id;
    body.type = CelestialBodyType::Star;
    body.ephemerisSource = CelestialBodyEphemerisSource::FixedEquatorial;
    body.fixedEquatorial = core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    return body;
}

[[nodiscard]] CelestialBody makeAstrometricStarBody(std::string id, const double rightAscensionHours)
{
    CelestialBody body = makeFixedStarBody(std::move(id), rightAscensionHours, 10.0);
    body.fixedEquatorial.reset();
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial =
            core::EquatorialCoordinate{
                .rightAscensionHours = rightAscensionHours,
                .declinationDeg = 10.0,
            },
        .referenceEpoch =
            AstronomicalEpoch{
                .julianDatePart1 = 2'451'545.0,
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tdb,
            },
        .properMotionRightAscensionMasPerYear = 0.0,
        .properMotionDeclinationMasPerYear = 0.0,
        .stellarParallaxMas = 100.0,
        .radialVelocityKmPerSecond = 0.0,
    };
    return body;
}

[[nodiscard]] CelestialBody makeUnsupportedBody()
{
    return {
        .id = "unresolved",
        .displayName = "Unresolved",
        .type = CelestialBodyType::DeepSkyObject,
        .ephemerisSource = CelestialBodyEphemerisSource::Unresolved,
    };
}

[[nodiscard]] core::SkyContext makeContext()
{
    core::SkyContext context;
    context.utcTime = core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] EphemerisRequest makeRequest()
{
    EphemerisRequest request;
    request.context = makeContext();
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = EphemerisCorrectionFlags::Apparent;
    return request;
}

[[nodiscard]] AstronomicalEpoch makeEpoch(
    const TimeScale timeScale,
    const int year,
    const int month,
    const int day,
    const int hour = 0,
    const int minute = 0,
    const int second = 0
)
{
    const std::optional<AstronomicalEpoch> epoch = astronomicalEpochFromCivilDateTime(CivilDateTime{
        .astronomicalYear = year,
        .month = month,
        .day = day,
        .hour = hour,
        .minute = minute,
        .second = second,
        .timeScale = timeScale,
    });
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] AstronomicalEpoch makeUtcEpoch(
    const int year, const int month, const int day, const int hour = 0, const int minute = 0, const int second = 0
)
{
    return makeEpoch(TimeScale::Utc, year, month, day, hour, minute, second);
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
makeLeapSecondProvider(const std::optional<AstronomicalEpoch>& referenceEpoch = std::nullopt)
{
    LeapSecondTableLoadOptions options;
    options.referenceEpoch = referenceEpoch;
    const LeapSecondTableLoadResult result = loadLeapSecondTableFromTextAsset(makeLeapSecondAsset(), options);
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] EphemerisTextDataAsset makeEarthOrientationAsset()
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

[[nodiscard]] std::shared_ptr<const IEarthOrientationProvider>
makeEarthOrientationProvider(const std::optional<AstronomicalEpoch>& referenceEpoch = std::nullopt)
{
    EarthOrientationDataLoadOptions options;
    options.referenceEpoch = referenceEpoch;
    const EarthOrientationDataLoadResult result =
        loadEarthOrientationDataFromTextAsset(makeEarthOrientationAsset(), options);
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
    const DeltaTDataLoadResult result = loadDeltaTDataFromTextAsset(makeDeltaTAsset());
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] HighPrecisionCalculatorResult makeCalculatorResult(
    const HighPrecisionComputationInput& input,
    const double rightAscensionHours,
    const double declinationDeg,
    const std::string_view provenance
)
{
    HighPrecisionCalculatorResult result;
    result.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    result.metadata.dataSourceProvenance = provenance;
    result.metadata.appliedCorrections = input.request.options.correctionFlags;
    return result;
}

class RecordingSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags;
        return makeCalculatorResult(input, 1.25, -2.5, "solar-system fake");
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class RecordingStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags;
        return makeCalculatorResult(input, 3.5, 42.0, "star-astrometry fake");
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class BatchRecordingStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_singleCallCount;
        return makeCalculatorResult(input, 21.0, -21.0, "single-star fallback");
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override
    {
        static_cast<void>(preparedRequestState);
        ++m_batchCallCount;
        m_lastBatchSize = arrays.size();
        m_lastRequestPart2 = request.epoch.julianDatePart2;

        std::vector<StarAstrometryBatchResult> results;
        results.reserve(arrays.size());
        for (std::size_t arrayIndex = 0; arrayIndex < arrays.size(); ++arrayIndex) {
            HighPrecisionCalculatorResult result;
            result.equatorial = core::EquatorialCoordinate{
                .rightAscensionHours =
                    arrays.referenceRightAscensionHours()[arrayIndex] + request.epoch.julianDatePart2,
                .declinationDeg = arrays.referenceDeclinationDegrees()[arrayIndex],
            };
            result.metadata.status = EphemerisResultStatus::Valid;
            result.metadata.dataSourceProvenance = "batch star astrometry";
            results.push_back(StarAstrometryBatchResult{
                .bodyIndex = arrays.bodyIndices()[arrayIndex],
                .result = result,
            });
        }
        return results;
    }

    [[nodiscard]] int singleCallCount() const noexcept
    {
        return m_singleCallCount;
    }

    [[nodiscard]] int batchCallCount() const noexcept
    {
        return m_batchCallCount;
    }

    [[nodiscard]] std::size_t lastBatchSize() const noexcept
    {
        return m_lastBatchSize;
    }

    [[nodiscard]] double lastRequestPart2() const noexcept
    {
        return m_lastRequestPart2;
    }

private:
    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
    mutable double m_lastRequestPart2 = 0.0;
};

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

class ThreadSafeEpochSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    explicit ThreadSafeEpochSolarSystemCalculator(const double baseRightAscensionHours)
        : m_baseRightAscensionHours(baseRightAscensionHours)
    {
    }

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        return makeCalculatorResult(
            input, m_baseRightAscensionHours + input.request.epoch.julianDatePart2, -2.5, "thread-safe fake"
        );
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount.load();
    }

private:
    double m_baseRightAscensionHours = 0.0;
    mutable std::atomic<int> m_callCount = 0;
};

class LongRangeFallbackKernelProvider final : public ICalcephKernelProvider {
public:
    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        ++m_callCount;
        m_lastEpoch = epoch;
        m_lastTargetNaifId = targetNaifId;
        m_lastCenterNaifId = centerNaifId;

        SolarSystemKernelStateResult result;
        result.positionAu = SolarSystemKernelVector{.xAu = 0.5, .yAu = 1.0, .zAu = 0.25};
        result.metadata.status = EphemerisResultStatus::OutOfRange;
        result.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
        result.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
        result.metadata.dataSourceProvenance = "missing DE441 long-range kernel; modern kernel fallback";
        result.metadata.effectiveDataValidityRange = EphemerisDateRange{
            .id = "de440-modern",
            .displayName = "DE440 modern range",
            .start = {.julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
            .end = {.julianDatePart1 = 2'700'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        };
        result.metadata.estimatedAngularUncertaintyArcsec = 3600.0;
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] int lastTargetNaifId() const noexcept
    {
        return m_lastTargetNaifId;
    }

    [[nodiscard]] int lastCenterNaifId() const noexcept
    {
        return m_lastCenterNaifId;
    }

    [[nodiscard]] AstronomicalEpoch lastEpoch() const noexcept
    {
        return m_lastEpoch;
    }

private:
    mutable int m_callCount = 0;
    mutable int m_lastTargetNaifId = 0;
    mutable int m_lastCenterNaifId = 0;
    mutable AstronomicalEpoch m_lastEpoch;
};

class RecordingApparentPlaceCalculator final : public IApparentPlaceCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_callCount;
        m_lastFlags = input.request.options.correctionFlags;

        HighPrecisionCalculatorResult result = calculatorResult;
        if (result.equatorial.has_value()) {
            result.equatorial->rightAscensionHours += 10.0;
        }
        result.metadata.appliedCorrections = input.request.options.correctionFlags;
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class BatchRecordingFrameTransformer final : public IFrameTransformer {
public:
    [[nodiscard]] CelestialFrameTransformResult transformCelestialVector(const CelestialFrameTransformRequest& request
    ) const override
    {
        ++m_singleCallCount;
        return transform(request.vector);
    }

    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transformCelestialVectors(const CelestialFrameBatchTransformRequest& request) const override
    {
        ++m_batchCallCount;
        m_lastBatchSize = request.vectors.size();

        std::vector<CelestialFrameTransformResult> results;
        results.reserve(request.vectors.size());
        for (const CelestialFrameVector& vector : request.vectors) {
            results.push_back(transform(vector));
        }
        return results;
    }

    [[nodiscard]] int singleCallCount() const noexcept
    {
        return m_singleCallCount;
    }

    [[nodiscard]] int batchCallCount() const noexcept
    {
        return m_batchCallCount;
    }

    [[nodiscard]] std::size_t lastBatchSize() const noexcept
    {
        return m_lastBatchSize;
    }

private:
    [[nodiscard]] static CelestialFrameTransformResult transform(const CelestialFrameVector& vector)
    {
        CelestialFrameTransformResult result;
        result.vector = vector;
        result.metadata.status = EphemerisResultStatus::Valid;
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
        result.metadata.dataSourceProvenance = "batch frame transformer fake";
        return result;
    }

    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
};

class BatchValidTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++m_callCount;
        TimeScaleConversionResult result;
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);

        TimeScaleConversionResult result;
        result.epoch.timeScale = targetScale;
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount = 0;
};

class RecordingResultBuilder final : public IEphemerisResultBuilder {
public:
    [[nodiscard]] CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_stateCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags;

        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        if (calculatorResult.equatorial.has_value()) {
            state.equatorial = *calculatorResult.equatorial;
        }
        if (calculatorResult.horizontal.has_value()) {
            state.horizontal = *calculatorResult.horizontal;
        }
        state.metadata = calculatorResult.metadata;
        state.metadata.estimatedAngularUncertaintyArcsec = 0.42;
        return state;
    }

    [[nodiscard]] CelestialBodyState buildUnsupportedState(const HighPrecisionComputationInput& input) const override
    {
        ++m_unsupportedCount;
        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        state.metadata.status = EphemerisResultStatus::Unsupported;
        state.metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
        state.metadata.dataSourceProvenance = "result-builder fake";
        return state;
    }

    [[nodiscard]] CelestialBodyState buildFailedState(const HighPrecisionComputationInput& input) const override
    {
        ++m_failedCount;
        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        state.metadata.status = EphemerisResultStatus::Failed;
        state.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        state.metadata.dataSourceProvenance = "result-builder fake";
        return state;
    }

    [[nodiscard]] int stateCount() const noexcept
    {
        return m_stateCount;
    }

    [[nodiscard]] int unsupportedCount() const noexcept
    {
        return m_unsupportedCount;
    }

    [[nodiscard]] int failedCount() const noexcept
    {
        return m_failedCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    [[nodiscard]] static CelestialBodyState makeEmptyState(const std::size_t bodyIndex) noexcept
    {
        CelestialBodyState state;
        state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
        state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
        state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
        return state;
    }

    mutable int m_stateCount = 0;
    mutable int m_unsupportedCount = 0;
    mutable int m_failedCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::NoCorrections;
};

class StubEarthOrientationProvider final : public skygate::ephemeris::IEarthOrientationProvider {
public:
    [[nodiscard]] const EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const EarthOrientationTableEntry> entries() const noexcept override
    {
        return {};
    }

private:
    EarthOrientationDataInfo m_dataInfo;
};

class StubAtmosphericRefractionCalculator final : public IAtmosphericRefractionCalculator {};

void mergeConversionMetadata(EphemerisResultMetadata& metadata, const TimeScaleConversionResult& conversion) noexcept
{
    if (conversion.status == TimeScaleConversionStatus::Failed) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return;
    }
    if (conversion.status == TimeScaleConversionStatus::Degraded && metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    }
    if (conversion.hasWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::EarthOrientationDataMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::DeltaTUnavailable)) {
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    }
    if (conversion.hasWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable)
        || conversion.hasWarning(TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData)) {
        metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    }
}

class MetadataFrameTransformer final : public IFrameTransformer {
public:
    explicit MetadataFrameTransformer(std::shared_ptr<const ITimeScaleService> timeScaleService)
        : m_timeScaleService(std::move(timeScaleService))
    {
    }

    [[nodiscard]] CelestialFrameTransformResult transformCelestialVector(const CelestialFrameTransformRequest& request
    ) const override
    {
        CelestialFrameTransformResult result;
        result.vector = request.vector;
        result.metadata.status = EphemerisResultStatus::Valid;
        result.metadata.dataSourceProvenance = "metadata frame transformer";

        const bool usesTerrestrialFrame = request.sourceFrame == CelestialReferenceFrame::Itrs
                                          || request.targetFrame == CelestialReferenceFrame::Itrs
                                          || request.sourceFrame == CelestialReferenceFrame::Tirs
                                          || request.targetFrame == CelestialReferenceFrame::Tirs;
        result.metadata.appliedCorrections = usesTerrestrialFrame ? EphemerisCorrectionFlags::EarthOrientation
                                                                  : EphemerisCorrectionFlags::PrecessionNutation;
        if (m_timeScaleService == nullptr) {
            result.vector.reset();
            result.metadata.status = EphemerisResultStatus::Failed;
            result.metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            return result;
        }

        const TimeScaleConversionResult conversion =
            m_timeScaleService->convert(request.epoch, usesTerrestrialFrame ? TimeScale::Ut1 : TimeScale::Tt);
        mergeConversionMetadata(result.metadata, conversion);
        if (!conversion.isSuccess()) {
            result.vector.reset();
        }
        return result;
    }

private:
    std::shared_ptr<const ITimeScaleService> m_timeScaleService;
};

[[nodiscard]] HighPrecisionCalculatorResult makeApparentPipelineInputResult()
{
    HighPrecisionCalculatorResult result;
    result.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 20.0,
    };
    result.observerRelativePositionAu = SolarSystemKernelVector{.xAu = 0.75, .yAu = 0.25, .zAu = 0.5};
    result.metadata.dataSourceProvenance = "fixture geometric solar-system state";
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::Geometric;
    return result;
}

[[nodiscard]] HighPrecisionEphemerisEngineDependencies makeDependencies(
    std::shared_ptr<ISolarSystemStateCalculator> solarSystemCalculator = {},
    std::shared_ptr<IStarAstrometryCalculator> starAstrometryCalculator = {},
    std::shared_ptr<IApparentPlaceCalculator> apparentPlaceCalculator = {},
    std::shared_ptr<IEphemerisResultBuilder> resultBuilder = {},
    std::shared_ptr<IEphemerisComputationCache> computationCache = {}
)
{
    HighPrecisionEphemerisEngineDependencies dependencies;
    dependencies.solarSystemStateCalculator = std::move(solarSystemCalculator);
    dependencies.starAstrometryCalculator = std::move(starAstrometryCalculator);
    dependencies.apparentPlaceCalculator = std::move(apparentPlaceCalculator);
    dependencies.resultBuilder = std::move(resultBuilder);
    dependencies.computationCache = std::move(computationCache);
    dependencies.dataSetInfo.id = "test-data";
    dependencies.dataSetInfo.displayName = "Test data";
    dependencies.dataSetInfo.version = "fixture";
    dependencies.dataSetInfo.provenance = "unit test";
    dependencies.dataSetInfo.dateRanges.push_back(EphemerisDateRange{
        .id = "modern",
        .displayName = "Modern",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    });
    return dependencies;
}

[[nodiscard]] HighPrecisionEphemerisEngineDependencies makeApparentPipelineDependencies(
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

    HighPrecisionEphemerisEngineDependencies dependencies =
        makeDependencies(std::move(solarSystemCalculator), {}, apparentPlaceCalculator);
    dependencies.timeScaleService = std::move(timeScaleService);
    dependencies.earthOrientationProvider = std::move(earthOrientationProviderForFrame);
    dependencies.frameTransformer = std::move(frameTransformer);
    return dependencies;
}

}  // namespace

class HighPrecisionEphemerisEngineTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesMetadataAndCapabilities();
    void dispatchesSolarSystemAndStarBodies();
    void usesBatchStarPathForFullFrameSnapshot();
    void batchesRepresentativeLargeCatalogWithoutSingleStarDispatch();
    void batchesRepresentativeLargeCatalogApparentPlaceTransformsOnce();
    void batchesTopocentricApparentPlaceRequestWideState();
    void reusesCachedFullFrameSnapshotsWithoutStaleResults();
    void reusesPreparedRequestStateAcrossSingleBodyComputations();
    void servesConcurrentReadOnlyComputationsFromCache();
    void isolatesCachedSnapshotsByDataSetRevision();
    void isolatesCachedSnapshotsByDataSetDateRangeContents();
    void isolatesCachedSnapshotsByCatalogContents();
    void fallsBackToSingleStarPathWhenBatchReturnsNoResults();
    void forwardsOptionsThroughCollaboratorsAndResultBuilder();
    void bypassesApparentPlaceForGeometricSolarSystemRequests();
    void bypassesApparentPlaceForGeometricStarRequests();
    void validatesRequestsBeforeDispatchingCalculators();
    void returnsStructuredUnsupportedStatus();
    void defaultResultBuilderAssemblesValidMetadata();
    void defaultResultBuilderTracksRequestedAppliedSkippedAndUnavailableCorrections();
    void defaultResultBuilderTurnsOutOfRangeFallbackIntoDegradedResult();
    void defaultResultBuilderPreservesOutOfRangeWithoutFallback();
    void defaultResultBuilderPreservesFailedResultsWithoutFallback();
    void defaultResultBuilderPreservesDegradedDataWarnings();
    void defaultResultBuilderAssemblesMissingLongRangeKernelFallback();
    void defaultResultBuilderAssemblesStaleEarthOrientationWarnings();
    void defaultResultBuilderAssemblesStaleLeapSecondWarnings();
    void defaultResultBuilderAssemblesAncientDeltaTFallbackWarnings();
};

void HighPrecisionEphemerisEngineTests::exposesMetadataAndCapabilities()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto dependencies = makeDependencies(solarSystemCalculator, starAstrometryCalculator);
    dependencies.earthOrientationProvider = std::make_shared<StubEarthOrientationProvider>();
    dependencies.atmosphericRefractionCalculator = std::make_shared<StubAtmosphericRefractionCalculator>();

    EphemerisEngineOptions options;
    options.engineKind = EphemerisEngineKind::Simple;
    options.correctionFlags = EphemerisCorrectionFlags::ApparentTopocentric;

    const HighPrecisionEphemerisEngine engine(bodies, options, std::move(dependencies));

    QCOMPARE(static_cast<std::uint8_t>(engine.kind()), static_cast<std::uint8_t>(EphemerisEngineKind::HighPrecision));
    QVERIFY(engine.name() == std::string_view{"High-precision ephemeris engine"});
    QCOMPARE(
        static_cast<std::uint8_t>(engine.options().engineKind),
        static_cast<std::uint8_t>(EphemerisEngineKind::HighPrecision)
    );

    const EphemerisCapabilities capabilities = engine.capabilities();
    QCOMPARE(
        static_cast<std::uint8_t>(capabilities.engineKind),
        static_cast<std::uint8_t>(EphemerisEngineKind::HighPrecision)
    );
    QVERIFY(capabilities.supportsSolarSystemBodies);
    QVERIFY(capabilities.supportsCatalogStars);
    QVERIFY(capabilities.supportsTopocentricPositions);
    QVERIFY(capabilities.supportsAtmosphericRefraction);
    QVERIFY(capabilities.supportsExtendedHistoricalRange);
    QCOMPARE(engine.supportedDateRanges().size(), std::size_t{1});
    QVERIFY(engine.dataSetInfo().id == std::string{"test-data"});
}

void HighPrecisionEphemerisEngineTests::dispatchesSolarSystemAndStarBodies()
{
    const std::array bodies{makeSunBody(), makeStarBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeDependencies(solarSystemCalculator, starAstrometryCalculator, apparentPlaceCalculator, resultBuilder)
    );

    const SkySnapshot snapshot = engine.compute(makeRequest());

    QCOMPARE(snapshot.states.size(), std::size_t{2});
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(starAstrometryCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 2);
    QCOMPARE(resultBuilder->stateCount(), 2);
    QVERIFY(solarSystemCalculator->lastBodyId() == std::string{"sun"});
    QVERIFY(starAstrometryCalculator->lastBodyId() == std::string{"vega"});
    QCOMPARE(snapshot.states[0].equatorial.rightAscensionHours, 11.25);
    QCOMPARE(snapshot.states[0].equatorial.declinationDeg, -2.5);
    QCOMPARE(snapshot.states[1].equatorial.rightAscensionHours, 13.5);
    QCOMPARE(snapshot.states[1].equatorial.declinationDeg, 42.0);
}

void HighPrecisionEphemerisEngineTests::usesBatchStarPathForFullFrameSnapshot()
{
    const std::array bodies{
        makeSunBody(),
        makeFixedStarBody("star-a", 2.0, 10.0),
        makeFixedStarBody("star-b", 5.0, -20.0),
        makeUnsupportedBody(),
    };
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<BatchRecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();

    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeDependencies(solarSystemCalculator, starAstrometryCalculator, apparentPlaceCalculator)
    );

    EphemerisRequest firstRequest = makeRequest();
    firstRequest.epoch.julianDatePart2 = 0.25;
    const SkySnapshot firstSnapshot = engine.compute(firstRequest);

    QCOMPARE(firstSnapshot.states.size(), std::size_t{4});
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(starAstrometryCalculator->lastBatchSize(), std::size_t{2});
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 3);
    QCOMPARE(firstSnapshot.states[1].bodyIndex, std::uint32_t{1});
    QCOMPARE(firstSnapshot.states[2].bodyIndex, std::uint32_t{2});
    QCOMPARE(firstSnapshot.states[1].equatorial.rightAscensionHours, 12.25);
    QCOMPARE(firstSnapshot.states[1].equatorial.declinationDeg, 10.0);
    QCOMPARE(firstSnapshot.states[2].equatorial.rightAscensionHours, 15.25);
    QCOMPARE(firstSnapshot.states[2].equatorial.declinationDeg, -20.0);
    QCOMPARE(firstSnapshot.states[3].metadata.status, EphemerisResultStatus::Unsupported);

    EphemerisRequest secondRequest = firstRequest;
    secondRequest.epoch.julianDatePart2 = 0.75;
    const SkySnapshot secondSnapshot = engine.compute(secondRequest);

    QCOMPARE(starAstrometryCalculator->batchCallCount(), 2);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(starAstrometryCalculator->lastRequestPart2(), 0.75);
    QCOMPARE(secondSnapshot.states[1].equatorial.rightAscensionHours, 12.75);
    QCOMPARE(secondSnapshot.states[2].equatorial.rightAscensionHours, 15.75);
}

void HighPrecisionEphemerisEngineTests::batchesRepresentativeLargeCatalogWithoutSingleStarDispatch()
{
    constexpr std::size_t kRepresentativeCatalogSize = 4'096U;
    std::vector<CelestialBody> bodies;
    bodies.reserve(kRepresentativeCatalogSize);
    for (std::size_t index = 0; index < kRepresentativeCatalogSize; ++index) {
        bodies.push_back(
            makeFixedStarBody("star-" + std::to_string(index), 1.0 + static_cast<double>(index) * 0.001, 5.0)
        );
    }
    auto starAstrometryCalculator = std::make_shared<BatchRecordingStarAstrometryCalculator>();

    const HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies({}, starAstrometryCalculator)
    );

    const SkySnapshot snapshot = engine.compute(makeRequest());

    QCOMPARE(snapshot.states.size(), kRepresentativeCatalogSize);
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);
    QCOMPARE(starAstrometryCalculator->lastBatchSize(), kRepresentativeCatalogSize);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(snapshot.states.front().bodyIndex, std::uint32_t{0});
    QCOMPARE(snapshot.states.back().bodyIndex, static_cast<std::uint32_t>(kRepresentativeCatalogSize - 1U));
}

void HighPrecisionEphemerisEngineTests::batchesRepresentativeLargeCatalogApparentPlaceTransformsOnce()
{
    constexpr std::size_t kRepresentativeCatalogSize = 4'096U;
    std::vector<CelestialBody> bodies;
    bodies.reserve(kRepresentativeCatalogSize);
    for (std::size_t index = 0; index < kRepresentativeCatalogSize; ++index) {
        bodies.push_back(
            makeFixedStarBody("star-" + std::to_string(index), 1.0 + static_cast<double>(index) * 0.001, 5.0)
        );
    }

    auto starAstrometryCalculator = std::make_shared<StarAstrometryCalculator>();
    auto frameTransformer = std::make_shared<BatchRecordingFrameTransformer>();
    auto apparentPlaceCalculator = std::make_shared<ApparentPlaceCalculator>(frameTransformer, nullptr, nullptr);

    const HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies({}, starAstrometryCalculator, apparentPlaceCalculator)
    );

    const SkySnapshot snapshot = engine.compute(makeRequest());

    QCOMPARE(snapshot.states.size(), kRepresentativeCatalogSize);
    QCOMPARE(frameTransformer->batchCallCount(), 1);
    QCOMPARE(frameTransformer->lastBatchSize(), kRepresentativeCatalogSize);
    QCOMPARE(frameTransformer->singleCallCount(), 0);
    QCOMPARE(snapshot.states.front().bodyIndex, std::uint32_t{0});
    QCOMPARE(snapshot.states.back().bodyIndex, static_cast<std::uint32_t>(kRepresentativeCatalogSize - 1U));
}

void HighPrecisionEphemerisEngineTests::batchesTopocentricApparentPlaceRequestWideState()
{
    constexpr std::size_t kRepresentativeCatalogSize = 4'096U;
    std::vector<CelestialBody> bodies;
    bodies.reserve(kRepresentativeCatalogSize);
    for (std::size_t index = 0; index < kRepresentativeCatalogSize; ++index) {
        bodies.push_back(
            makeFixedStarBody("star-" + std::to_string(index), 1.0 + static_cast<double>(index) * 0.001, 5.0)
        );
    }

    auto timeScaleService = std::make_shared<BatchValidTimeScaleService>();
    auto starAstrometryCalculator = std::make_shared<StarAstrometryCalculator>();
    auto frameTransformer = std::make_shared<BatchRecordingFrameTransformer>();
    auto apparentPlaceCalculator =
        std::make_shared<ApparentPlaceCalculator>(frameTransformer, timeScaleService, makeEarthOrientationProvider());

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::Topocentric;
    const HighPrecisionEphemerisEngine engine(
        bodies, request.options, makeDependencies({}, starAstrometryCalculator, apparentPlaceCalculator)
    );

    const SkySnapshot snapshot = engine.compute(request);

    QCOMPARE(snapshot.states.size(), kRepresentativeCatalogSize);
    QCOMPARE(timeScaleService->callCount(), 1);
    QCOMPARE(frameTransformer->batchCallCount(), 3);
    QCOMPARE(frameTransformer->lastBatchSize(), kRepresentativeCatalogSize);
    QCOMPARE(frameTransformer->singleCallCount(), 0);
    QCOMPARE(snapshot.states.front().bodyIndex, std::uint32_t{0});
    QCOMPARE(snapshot.states.back().bodyIndex, static_cast<std::uint32_t>(kRepresentativeCatalogSize - 1U));
}

void HighPrecisionEphemerisEngineTests::reusesCachedFullFrameSnapshotsWithoutStaleResults()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<ThreadSafeEpochSolarSystemCalculator>(4.0);
    auto computationCache = std::make_shared<EphemerisComputationCache>();

    const HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies(solarSystemCalculator, {}, {}, {}, computationCache)
    );

    EphemerisRequest firstRequest = makeRequest();
    firstRequest.epoch.julianDatePart2 = 0.25;
    const SkySnapshot firstSnapshot = engine.compute(firstRequest);
    const SkySnapshot repeatedSnapshot = engine.compute(firstRequest);

    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(firstSnapshot.states[0].equatorial.rightAscensionHours, 4.25);
    QCOMPARE(repeatedSnapshot.states[0].equatorial.rightAscensionHours, 4.25);

    EphemerisRequest changedRequest = firstRequest;
    changedRequest.epoch.julianDatePart2 = 0.75;
    const SkySnapshot changedSnapshot = engine.compute(changedRequest);

    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(changedSnapshot.states[0].equatorial.rightAscensionHours, 4.75);

    const auto cachedState = engine.computeBodyState(firstRequest, std::size_t{0});

    QVERIFY(cachedState.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(cachedState->equatorial.rightAscensionHours, 4.25);

    const auto cachedStateById = engine.computeBodyState(firstRequest, std::string_view{"sun"});

    QVERIFY(cachedStateById.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 2);
    QCOMPARE(cachedStateById->equatorial.rightAscensionHours, 4.25);
}

void HighPrecisionEphemerisEngineTests::reusesPreparedRequestStateAcrossSingleBodyComputations()
{
    const std::array bodies{makeAstrometricStarBody("star-a", 2.0), makeAstrometricStarBody("star-b", 5.0)};
    auto kernelProvider = std::make_shared<LongRangeFallbackKernelProvider>();
    auto starAstrometryCalculator = std::make_shared<StarAstrometryCalculator>(kernelProvider);
    auto computationCache = std::make_shared<EphemerisComputationCache>();
    HighPrecisionEphemerisEngineDependencies dependencies =
        makeDependencies({}, starAstrometryCalculator, {}, {}, computationCache);
    dependencies.calcephKernelProvider = kernelProvider;

    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, std::move(dependencies));

    const auto firstState = engine.computeBodyState(makeRequest(), std::string_view{"star-a"});
    const auto secondState = engine.computeBodyState(makeRequest(), std::string_view{"star-b"});

    QVERIFY(firstState.has_value());
    QVERIFY(secondState.has_value());
    QCOMPARE(kernelProvider->callCount(), 1);
}

void HighPrecisionEphemerisEngineTests::servesConcurrentReadOnlyComputationsFromCache()
{
    constexpr std::size_t kThreadCount = 8U;
    constexpr int kComputesPerThread = 32;

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<ThreadSafeEpochSolarSystemCalculator>(7.0);
    auto computationCache = std::make_shared<EphemerisComputationCache>();

    const HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies(solarSystemCalculator, {}, {}, {}, computationCache)
    );

    EphemerisRequest request = makeRequest();
    request.epoch.julianDatePart2 = 0.125;
    const SkySnapshot warmedSnapshot = engine.compute(request);
    QCOMPARE(warmedSnapshot.states[0].equatorial.rightAscensionHours, 7.125);
    QCOMPARE(solarSystemCalculator->callCount(), 1);

    std::array<bool, kThreadCount> threadResults{};
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (std::size_t threadIndex = 0U; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([&engine, &request, &threadResults, threadIndex]() {
            bool matched = true;
            for (int iteration = 0; iteration < kComputesPerThread; ++iteration) {
                const SkySnapshot snapshot = engine.compute(request);
                matched = matched && snapshot.states.size() == 1U
                          && snapshot.states[0].equatorial.rightAscensionHours == 7.125;
            }
            threadResults[threadIndex] = matched;
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    for (const bool threadResult : threadResults) {
        QVERIFY(threadResult);
    }
    QCOMPARE(solarSystemCalculator->callCount(), 1);
}

void HighPrecisionEphemerisEngineTests::isolatesCachedSnapshotsByDataSetRevision()
{
    EphemerisComputationCache computationCache;
    const std::vector<CelestialBody> bodies{makeSunBody()};
    const EphemerisRequest request = makeRequest();

    EphemerisDataSetInfo oldDataSet;
    oldDataSet.id = "test-data";
    oldDataSet.version = "old";
    oldDataSet.provenance = "old snapshot";

    EphemerisDataSetInfo newDataSet = oldDataSet;
    newDataSet.version = "new";
    newDataSet.provenance = "new snapshot";

    SkySnapshot oldSnapshot;
    oldSnapshot.states.push_back(CelestialBodyState{
        .bodyIndex = 0U,
        .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
    });
    SkySnapshot newSnapshot;
    newSnapshot.states.push_back(CelestialBodyState{
        .bodyIndex = 0U,
        .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
    });

    computationCache.storeSnapshot(request, bodies, oldDataSet, oldSnapshot);
    computationCache.storeSnapshot(request, bodies, newDataSet, newSnapshot);

    const std::optional<SkySnapshot> cachedOldSnapshot = computationCache.findSnapshot(request, bodies, oldDataSet);
    const std::optional<SkySnapshot> cachedNewSnapshot = computationCache.findSnapshot(request, bodies, newDataSet);

    QVERIFY(cachedOldSnapshot.has_value());
    QVERIFY(cachedNewSnapshot.has_value());
    QCOMPARE(cachedOldSnapshot->states[0].equatorial.rightAscensionHours, 1.0);
    QCOMPARE(cachedNewSnapshot->states[0].equatorial.rightAscensionHours, 3.0);

    newSnapshot.states[0].equatorial.rightAscensionHours = 99.0;
    const std::optional<SkySnapshot> cachedOldSnapshotAfterMutation =
        computationCache.findSnapshot(request, bodies, oldDataSet);

    QVERIFY(cachedOldSnapshotAfterMutation.has_value());
    QCOMPARE(cachedOldSnapshotAfterMutation->states[0].equatorial.rightAscensionHours, 1.0);
}

void HighPrecisionEphemerisEngineTests::isolatesCachedSnapshotsByDataSetDateRangeContents()
{
    EphemerisComputationCache computationCache;
    const std::vector<CelestialBody> bodies{makeSunBody()};
    const EphemerisRequest request = makeRequest();

    EphemerisDataSetInfo oldDataSet;
    oldDataSet.id = "test-data";
    oldDataSet.version = "same-version";
    oldDataSet.provenance = "same provenance";
    oldDataSet.dateRanges.push_back(EphemerisDateRange{
        .id = "modern",
        .displayName = "Modern",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    });

    EphemerisDataSetInfo newDataSet = oldDataSet;
    newDataSet.dateRanges[0].end.julianDatePart1 = 2'600'000.5;

    SkySnapshot oldSnapshot;
    oldSnapshot.states.push_back(CelestialBodyState{
        .bodyIndex = 0U,
        .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
    });
    SkySnapshot newSnapshot;
    newSnapshot.states.push_back(CelestialBodyState{
        .bodyIndex = 0U,
        .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
    });

    computationCache.storeSnapshot(request, bodies, oldDataSet, oldSnapshot);
    computationCache.storeSnapshot(request, bodies, newDataSet, newSnapshot);

    const std::optional<SkySnapshot> cachedOldSnapshot = computationCache.findSnapshot(request, bodies, oldDataSet);
    const std::optional<SkySnapshot> cachedNewSnapshot = computationCache.findSnapshot(request, bodies, newDataSet);

    QVERIFY(cachedOldSnapshot.has_value());
    QVERIFY(cachedNewSnapshot.has_value());
    QCOMPARE(cachedOldSnapshot->states[0].equatorial.rightAscensionHours, 1.0);
    QCOMPARE(cachedNewSnapshot->states[0].equatorial.rightAscensionHours, 3.0);
}

void HighPrecisionEphemerisEngineTests::isolatesCachedSnapshotsByCatalogContents()
{
    EphemerisComputationCache computationCache;
    std::vector<CelestialBody> bodies{makeFixedStarBody("star-a", 1.0, 2.0)};
    const EphemerisRequest request = makeRequest();
    const EphemerisDataSetInfo dataSet = makeDependencies().dataSetInfo;

    SkySnapshot oldSnapshot;
    oldSnapshot.states.push_back(CelestialBodyState{
        .bodyIndex = 0U,
        .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
    });
    computationCache.storeSnapshot(request, bodies, dataSet, oldSnapshot);

    bodies[0] = makeFixedStarBody("star-b", 5.0, 6.0);
    const std::optional<SkySnapshot> cachedSnapshot = computationCache.findSnapshot(request, bodies, dataSet);

    QVERIFY(!cachedSnapshot.has_value());
}

void HighPrecisionEphemerisEngineTests::fallsBackToSingleStarPathWhenBatchReturnsNoResults()
{
    const std::array bodies{
        makeFixedStarBody("star-a", 2.0, 10.0),
        makeFixedStarBody("star-b", 5.0, -20.0),
    };
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();

    const HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies({}, starAstrometryCalculator)
    );

    const SkySnapshot snapshot = engine.compute(makeRequest());

    QCOMPARE(snapshot.states.size(), std::size_t{2});
    QCOMPARE(starAstrometryCalculator->callCount(), 2);
    QCOMPARE(snapshot.states[0].equatorial.rightAscensionHours, 3.5);
    QCOMPARE(snapshot.states[1].equatorial.rightAscensionHours, 3.5);
}

void HighPrecisionEphemerisEngineTests::forwardsOptionsThroughCollaboratorsAndResultBuilder()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::EarthOrientation;

    EphemerisEngineOptions engineOptions = request.options;
    engineOptions.correctionFlags = EphemerisCorrectionFlags::AtmosphericRefraction;

    const HighPrecisionEphemerisEngine engine(
        bodies, engineOptions, makeDependencies(solarSystemCalculator, {}, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint32_t>(solarSystemCalculator->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(apparentPlaceCalculator->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(resultBuilder->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(request.options.correctionFlags)
    );
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QVERIFY(state->metadata.estimatedAngularUncertaintyArcsec.has_value());
    QCOMPARE(*state->metadata.estimatedAngularUncertaintyArcsec, 0.42);
}

void HighPrecisionEphemerisEngineTests::bypassesApparentPlaceForGeometricSolarSystemRequests()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::Geometric;

    const HighPrecisionEphemerisEngine engine(
        bodies, request.options, makeDependencies(solarSystemCalculator, {}, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->stateCount(), 1);
    QCOMPARE(state->equatorial.rightAscensionHours, 1.25);
    QCOMPARE(state->equatorial.declinationDeg, -2.5);
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::Geometric)
    );
}

void HighPrecisionEphemerisEngineTests::bypassesApparentPlaceForGeometricStarRequests()
{
    const std::array bodies{makeStarBody()};
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::Geometric;

    const HighPrecisionEphemerisEngine engine(
        bodies, request.options, makeDependencies({}, starAstrometryCalculator, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(starAstrometryCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->stateCount(), 1);
    QCOMPARE(state->equatorial.rightAscensionHours, 3.5);
    QCOMPARE(state->equatorial.declinationDeg, 42.0);
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::Geometric)
    );
}

void HighPrecisionEphemerisEngineTests::validatesRequestsBeforeDispatchingCalculators()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    HighPrecisionEphemerisEngine engine(
        bodies, makeRequest().options, makeDependencies(solarSystemCalculator, {}, {}, resultBuilder)
    );

    EphemerisRequest request = makeRequest();
    request.epoch.julianDatePart1 = std::numeric_limits<double>::quiet_NaN();

    const auto state = engine.computeBodyState(request, "sun");

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->failedCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::ComputationFailed));
}

void HighPrecisionEphemerisEngineTests::returnsStructuredUnsupportedStatus()
{
    const std::array bodies{makeUnsupportedBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeDependencies(solarSystemCalculator, starAstrometryCalculator, {}, resultBuilder)
    );

    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 0);
    QCOMPARE(starAstrometryCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->unsupportedCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Unsupported)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::UnsupportedBody));
    QVERIFY(!state->metadata.dataSourceProvenance.empty());
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderAssemblesValidMetadata()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 5.0,
    };
    calculatorResult.horizontal = core::HorizontalCoordinate{
        .altitudeDeg = 35.0,
        .azimuthDeg = 180.0,
    };
    calculatorResult.metadata.dataSourceProvenance = "unit test kernel";
    calculatorResult.metadata.effectiveDataValidityRange = EphemerisDateRange{
        .id = "modern",
        .displayName = "Modern range",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };
    calculatorResult.metadata.estimatedAngularUncertaintyArcsec = 0.12;
    calculatorResult.metadata.appliedCorrections = EphemerisCorrectionFlags::LightTime;

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    const auto state = engine.computeBodyState(makeRequest(), "sun");

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QVERIFY(solarSystemCalculator->lastBodyId() == std::string{"sun"});
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
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
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::LightTime));
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderTracksRequestedAppliedSkippedAndUnavailableCorrections()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 4.0,
        .declinationDeg = 5.0,
    };
    calculatorResult.metadata.status = EphemerisResultStatus::Degraded;
    calculatorResult.metadata.appliedCorrections = EphemerisCorrectionFlags::LightTime;
    calculatorResult.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::StellarAberration);

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    EphemerisRequest request = makeRequest();
    request.options.correctionFlags = EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::StellarAberration
                                      | EphemerisCorrectionFlags::GravitationalLightDeflection;

    const auto state = engine.computeBodyState(request, "sun");

    QVERIFY(state.has_value());
    QCOMPARE(state->metadata.requestedCorrections, request.options.correctionFlags);
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::LightTime));
    QVERIFY(!hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::StellarAberration));
    QVERIFY(hasCorrectionFlag(state->metadata.unavailableCorrections, EphemerisCorrectionFlags::StellarAberration));
    QVERIFY(!hasCorrectionFlag(state->metadata.unavailableCorrections, EphemerisCorrectionFlags::LightTime));
    QVERIFY(
        hasCorrectionFlag(state->metadata.skippedCorrections, EphemerisCorrectionFlags::GravitationalLightDeflection)
    );
    QVERIFY(!hasCorrectionFlag(state->metadata.skippedCorrections, EphemerisCorrectionFlags::LightTime));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderTurnsOutOfRangeFallbackIntoDegradedResult()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 6.0,
        .declinationDeg = -7.0,
    };
    calculatorResult.metadata.status = EphemerisResultStatus::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    calculatorResult.metadata.dataSourceProvenance = "missing DE441 fallback";

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"missing DE441 fallback"});
    QCOMPARE(state->equatorial.rightAscensionHours, 6.0);
    QCOMPARE(state->equatorial.declinationDeg, -7.0);
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderPreservesOutOfRangeWithoutFallback()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.metadata.status = EphemerisResultStatus::OutOfRange;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    calculatorResult.metadata.dataSourceProvenance = "kernel out of range";

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::OutOfRange)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"kernel out of range"});
    QVERIFY(std::isnan(state->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(state->equatorial.declinationDeg));
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderPreservesFailedResultsWithoutFallback()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.metadata.status = EphemerisResultStatus::Failed;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
    calculatorResult.metadata.dataSourceProvenance = "missing kernel";

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::ComputationFailed));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"missing kernel"});
    QVERIFY(std::isnan(state->equatorial.rightAscensionHours));
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderPreservesDegradedDataWarnings()
{
    HighPrecisionCalculatorResult calculatorResult;
    calculatorResult.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 8.0,
        .declinationDeg = 9.0,
    };
    calculatorResult.metadata.status = EphemerisResultStatus::Degraded;
    calculatorResult.metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    calculatorResult.metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    calculatorResult.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
    calculatorResult.metadata.dataSourceProvenance = "stale EOP, stale leap-second, Delta T fallback";

    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(std::move(calculatorResult));
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    const auto state = engine.computeBodyState(makeRequest(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"stale EOP, stale leap-second, Delta T fallback"});
    QCOMPARE(state->equatorial.rightAscensionHours, 8.0);
    QCOMPARE(state->equatorial.declinationDeg, 9.0);
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderAssemblesMissingLongRangeKernelFallback()
{
    const std::array bodies{makeSunBody()};
    auto kernelProvider = std::make_shared<LongRangeFallbackKernelProvider>();
    auto solarSystemCalculator = std::make_shared<SolarSystemStateCalculator>(kernelProvider);
    const HighPrecisionEphemerisEngine engine(bodies, makeRequest().options, makeDependencies(solarSystemCalculator));

    EphemerisRequest request = makeRequest();
    request.epoch = makeEpoch(TimeScale::Tdb, -5000, 1, 1);
    request.options.correctionFlags = EphemerisCorrectionFlags::Geometric;

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(kernelProvider->callCount(), 1);
    QCOMPARE(kernelProvider->lastTargetNaifId(), 10);
    QCOMPARE(kernelProvider->lastCenterNaifId(), 399);
    QCOMPARE(
        static_cast<std::uint8_t>(kernelProvider->lastEpoch().timeScale), static_cast<std::uint8_t>(TimeScale::Tdb)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::MissingEphemerisData));
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

void HighPrecisionEphemerisEngineTests::defaultResultBuilderAssemblesStaleEarthOrientationWarnings()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(makeApparentPipelineInputResult());
    const std::shared_ptr<const IEarthOrientationProvider> staleEarthOrientationProvider =
        makeEarthOrientationProvider(makeUtcEpoch(2026, 8, 1));
    const auto timeScaleService = std::make_shared<LeapSecondTimeScaleService>(
        makeLeapSecondProvider(), TimeScaleServiceOptions{}, staleEarthOrientationProvider
    );
    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeApparentPipelineDependencies(
            solarSystemCalculator, timeScaleService, staleEarthOrientationProvider, staleEarthOrientationProvider
        )
    );

    EphemerisRequest request = makeRequest();
    request.epoch = makeUtcEpoch(2026, 4, 1);
    request.options.correctionFlags = EphemerisCorrectionFlags::Topocentric;
    request.options.enableAtmosphericRefraction = false;

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(state->metadata.dataSourceProvenance == std::string{"fixture geometric solar-system state"});
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::EarthOrientation));
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
    QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(state->horizontal.azimuthDeg));
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderAssemblesStaleLeapSecondWarnings()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(makeApparentPipelineInputResult());
    const auto timeScaleService =
        std::make_shared<LeapSecondTimeScaleService>(makeLeapSecondProvider(makeUtcEpoch(2027, 2, 1)));
    const HighPrecisionEphemerisEngine engine(
        bodies,
        makeRequest().options,
        makeApparentPipelineDependencies(solarSystemCalculator, timeScaleService, nullptr, nullptr)
    );

    EphemerisRequest request = makeRequest();
    request.epoch = makeUtcEpoch(2026, 4, 1);
    request.options.correctionFlags = EphemerisCorrectionFlags::Apparent;
    request.options.enableAtmosphericRefraction = false;

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
}

void HighPrecisionEphemerisEngineTests::defaultResultBuilderAssemblesAncientDeltaTFallbackWarnings()
{
    const std::array bodies{makeSunBody()};
    auto solarSystemCalculator = std::make_shared<StaticSolarSystemCalculator>(makeApparentPipelineInputResult());
    const std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider = makeEarthOrientationProvider();
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
        bodies,
        makeRequest().options,
        makeApparentPipelineDependencies(solarSystemCalculator, timeScaleService, nullptr, nullptr)
    );

    EphemerisRequest request = makeRequest();
    request.epoch = makeUtcEpoch(-5000, 1, 1);
    request.options.correctionFlags = EphemerisCorrectionFlags::Topocentric;
    request.options.enableAtmosphericRefraction = false;

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::DataOutOfRange));
    QVERIFY(state->metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::EarthOrientation));
    QVERIFY(hasCorrectionFlag(state->metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
    QVERIFY(std::isfinite(state->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(state->equatorial.declinationDeg));
    QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(state->horizontal.azimuthDeg));
}

QTEST_APPLESS_MAIN(HighPrecisionEphemerisEngineTests)

#include "HighPrecisionEphemerisEngineTests.moc"
