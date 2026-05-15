#include "engine/highprecision/FrameTransformer.hpp"

#include "skygate/ephemeris/LeapSecondProvider.hpp"
#include "skygate/ephemeris/TimeScaleService.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

constexpr double kTolerance = 1.0e-14;

[[nodiscard]] AstronomicalEpoch sofaReferenceTtEpoch() noexcept
{
    return {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = TimeScale::Tt,
    };
}

[[nodiscard]] AstronomicalEpoch utcEpoch() noexcept
{
    return {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'735.9992,
        .timeScale = TimeScale::Utc,
    };
}

[[nodiscard]] AstronomicalEpoch sofaReferenceUtcEpoch() noexcept
{
    return {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = TimeScale::Utc,
    };
}

[[nodiscard]] AstronomicalEpoch sofaReferenceUt1Epoch() noexcept
{
    return {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = TimeScale::Ut1,
    };
}

void compareVector(const CelestialFrameVector& actual, const CelestialFrameVector& expected, const double tolerance)
{
    QVERIFY(std::abs(actual.x - expected.x) <= tolerance);
    QVERIFY(std::abs(actual.y - expected.y) <= tolerance);
    QVERIFY(std::abs(actual.z - expected.z) <= tolerance);
}

class RecordingTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++convertCallCount;
        lastEpoch = epoch;
        lastTargetScale = targetScale;
        if (targetScale == TimeScale::Tt && ttResult.has_value()) {
            return *ttResult;
        }
        if (targetScale == TimeScale::Utc && utcResult.has_value()) {
            return *utcResult;
        }
        if (targetScale == TimeScale::Ut1 && ut1Result.has_value()) {
            return *ut1Result;
        }
        return nextResult;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

    mutable int convertCallCount = 0;
    mutable AstronomicalEpoch lastEpoch;
    mutable TimeScale lastTargetScale = TimeScale::Utc;
    TimeScaleConversionResult nextResult;
    std::optional<TimeScaleConversionResult> ttResult;
    std::optional<TimeScaleConversionResult> utcResult;
    std::optional<TimeScaleConversionResult> ut1Result;
};

class RecordingEarthOrientationProvider final : public IEarthOrientationProvider {
public:
    explicit RecordingEarthOrientationProvider(std::shared_ptr<const IEarthOrientationProvider> provider)
        : m_provider(std::move(provider))
    {
    }

    [[nodiscard]] const EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        ++dataInfoCallCount;
        return m_provider->dataInfo();
    }

    [[nodiscard]] std::span<const EarthOrientationTableEntry> entries() const noexcept override
    {
        ++entriesCallCount;
        return m_provider->entries();
    }

    mutable int dataInfoCallCount = 0;
    mutable int entriesCallCount = 0;

private:
    std::shared_ptr<const IEarthOrientationProvider> m_provider;
};

[[nodiscard]] TimeScaleConversionResult validTtConversionResult()
{
    return {
        .epoch = sofaReferenceTtEpoch(),
        .status = TimeScaleConversionStatus::Valid,
        .diagnosticText = "unit test conversion",
    };
}

[[nodiscard]] TimeScaleConversionResult validUtcConversionResult()
{
    return {
        .epoch = sofaReferenceUtcEpoch(),
        .status = TimeScaleConversionStatus::Valid,
        .diagnosticText = "unit test UTC conversion",
    };
}

[[nodiscard]] TimeScaleConversionResult validUt1ConversionResult()
{
    return {
        .epoch = sofaReferenceUt1Epoch(),
        .status = TimeScaleConversionStatus::Valid,
        .diagnosticText = "unit test UT1 conversion",
    };
}

[[nodiscard]] std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider(
    const bool predicted = false,
    const EarthOrientationDataStatus status = EarthOrientationDataStatus::Available,
    const bool estimated = false
)
{
    EarthOrientationDataInfo info;
    info.version = "unit-test-eop";
    info.provenance = "SOFA t_erfa_c polar-motion fixture";
    info.status = status;
    info.diagnosticText = "Earth-orientation data loaded.";

    EarthOrientationTableEntry entry;
    entry.effectiveUtcEpoch = sofaReferenceUtcEpoch();
    entry.ut1MinusUtcSeconds = 0.0;
    entry.polarMotionXArcseconds = 0.05260995057240829;
    entry.polarMotionYArcseconds = 0.38372663963244913;
    entry.predicted = predicted;
    entry.estimated = estimated;

    return std::make_shared<TableBackedEarthOrientationProvider>(
        std::move(info), std::vector<EarthOrientationTableEntry>{entry}
    );
}

[[nodiscard]] std::shared_ptr<const ILeapSecondProvider> leapSecondProvider()
{
    const LeapSecondTableLoadResult result = loadLeapSecondTableFromTextAsset(
        EphemerisTextDataAsset{
            .id = "leap-seconds",
            .version = "unit-test-leap-seconds",
            .provenance = "unit test leap-second data",
            .content = "#@ version unit-test-leap-seconds\n"
                       "#@ source unit test\n"
                       "#@ expires 2027-01-01\n"
                       "effective_utc_date,tai_minus_utc\n"
                       "1972-01-01,10\n"
                       "1972-07-01,11\n"
                       "2015-07-01,36\n"
                       "2017-01-01,37\n",
        }
    );
    Q_ASSERT(result.isSuccess());
    return result.provider;
}

[[nodiscard]] std::shared_ptr<RecordingTimeScaleService> frameTimeScaleService()
{
    auto service = std::make_shared<RecordingTimeScaleService>();
    service->ttResult = validTtConversionResult();
    service->utcResult = validUtcConversionResult();
    service->ut1Result = validUt1ConversionResult();
    return service;
}

}  // namespace

class FrameTransformerTests final : public QObject {
    Q_OBJECT

private slots:
    void transformsGcrsToCirsAgainstSofaReference();
    void roundTripsGcrsAndCirsVectors();
    void treatsIcrsAndGcrsAsIdentityCelestialAxes();
    void usesTimeScaleServiceForNonTtEpochs();
    void reportsMissingTimeScaleServiceForNonTtCirsTransforms();
    void rejectsUnsupportedTrueEquatorAndEquinoxTerrestrialTransforms();
    void transformsCirsToTirsAgainstSofaReference();
    void transformsTirsToItrsAgainstSofaReference();
    void transformsGcrsToItrsAgainstSofaReference();
    void recordsPerStageMetadataForComposedTransforms();
    void reusesTimeScaleConversionsAcrossComposedStages();
    void reusesEarthOrientationSampleAcrossComposedTerrestrialStages();
    void preservesIcrsSourceFrameInComposedStageMetadata();
    void preservesIcrsTargetFrameInComposedStageMetadata();
    void recordsUnavailableStageWhenTransformCannotBeComputed();
    void acceptsItrsTransformWithPredictedEarthOrientationData();
    void degradesItrsTransformForStaleEarthOrientationData();
    void degradesItrsTransformForEstimatedEarthOrientationData();
    void degradesItrsTransformForMissingEarthOrientationData();
};

void FrameTransformerTests::transformsGcrsToCirsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Cirs,
            .epoch = sofaReferenceTtEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    compareVector(
        *result.vector,
        {
            .x = 0.9999998323037166,
            .y = -0.23842662278707525e-7,
            .z = 0.5791308472168153e-3,
        },
        kTolerance
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::PrecessionNutation)
    );
}

void FrameTransformerTests::roundTripsGcrsAndCirsVectors()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformRequest gcrsToCirsRequest{
        .sourceFrame = CelestialReferenceFrame::Gcrs,
        .targetFrame = CelestialReferenceFrame::Cirs,
        .epoch = sofaReferenceTtEpoch(),
        .vector = {.x = 0.4, .y = -0.2, .z = 0.9},
    };

    const CelestialFrameTransformResult cirs = transformer.transformCelestialVector(gcrsToCirsRequest);
    QVERIFY(cirs.vector.has_value());

    const CelestialFrameTransformResult gcrs = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Cirs,
            .targetFrame = CelestialReferenceFrame::Gcrs,
            .epoch = sofaReferenceTtEpoch(),
            .vector = *cirs.vector,
        }
    );

    QVERIFY(gcrs.vector.has_value());
    compareVector(*gcrs.vector, gcrsToCirsRequest.vector, 2.0e-14);
}

void FrameTransformerTests::treatsIcrsAndGcrsAsIdentityCelestialAxes()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Icrs,
            .targetFrame = CelestialReferenceFrame::Gcrs,
            .epoch =
                {
                    .julianDatePart1 = std::numeric_limits<double>::quiet_NaN(),
                    .julianDatePart2 = std::numeric_limits<double>::quiet_NaN(),
                    .timeScale = TimeScale::Utc,
                },
            .vector = {.x = 0.25, .y = 0.5, .z = -0.75},
        }
    );

    QVERIFY(result.vector.has_value());
    compareVector(*result.vector, {.x = 0.25, .y = 0.5, .z = -0.75}, 0.0);
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::NoCorrections)
    );
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(1U));
    QVERIFY(!result.stages.front().applied);
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Icrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().metadata.status),
        static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages.front().metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::NoCorrections)
    );
    QVERIFY(!result.stages.front().metadata.dataSourceProvenance.empty());
}

void FrameTransformerTests::usesTimeScaleServiceForNonTtEpochs()
{
    auto service = std::make_shared<RecordingTimeScaleService>();
    service->nextResult = validTtConversionResult();
    const ErfaFrameTransformer transformer(service);
    const AstronomicalEpoch epoch = utcEpoch();

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Cirs,
            .epoch = epoch,
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(service->convertCallCount, 1);
    QCOMPARE(static_cast<std::uint8_t>(service->lastTargetScale), static_cast<std::uint8_t>(TimeScale::Tt));
    QCOMPARE(service->lastEpoch.julianDatePart1, epoch.julianDatePart1);
    QCOMPARE(service->lastEpoch.julianDatePart2, epoch.julianDatePart2);
}

void FrameTransformerTests::reportsMissingTimeScaleServiceForNonTtCirsTransforms()
{
    const ErfaFrameTransformer transformer(nullptr);

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Cirs,
            .epoch = utcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(!result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
}

void FrameTransformerTests::rejectsUnsupportedTrueEquatorAndEquinoxTerrestrialTransforms()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());
    const std::array<std::pair<CelestialReferenceFrame, CelestialReferenceFrame>, 4U> cases{{
        {CelestialReferenceFrame::TrueEquatorAndEquinox, CelestialReferenceFrame::Tirs},
        {CelestialReferenceFrame::Tirs, CelestialReferenceFrame::TrueEquatorAndEquinox},
        {CelestialReferenceFrame::TrueEquatorAndEquinox, CelestialReferenceFrame::Itrs},
        {CelestialReferenceFrame::Itrs, CelestialReferenceFrame::TrueEquatorAndEquinox},
    }};

    for (const auto& [sourceFrame, targetFrame] : cases) {
        const CelestialFrameTransformResult result = transformer.transformCelestialVector(
            CelestialFrameTransformRequest{
                .sourceFrame = sourceFrame,
                .targetFrame = targetFrame,
                .epoch = sofaReferenceUtcEpoch(),
                .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
            }
        );

        QVERIFY(!result.vector.has_value());
        QCOMPARE(
            static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
        );
        QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
        QCOMPARE(
            static_cast<std::uint32_t>(result.metadata.appliedCorrections),
            static_cast<std::uint32_t>(EphemerisCorrectionFlags::NoCorrections)
        );
        QCOMPARE(result.stages.size(), static_cast<std::size_t>(0U));
    }
}

void FrameTransformerTests::transformsCirsToTirsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Cirs,
            .targetFrame = CelestialReferenceFrame::Tirs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    compareVector(
        *result.vector,
        {
            .x = -0.18103321990176477,
            .y = -0.98347698157709784,
            .z = 0.0,
        },
        kTolerance
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::EarthOrientation)
    );
}

void FrameTransformerTests::transformsTirsToItrsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Tirs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    compareVector(
        *result.vector,
        {
            .x = 0.9999999999999674721,
            .y = 0.1414624947957029801e-10,
            .z = -0.2550602379741215021e-6,
        },
        kTolerance
    );
}

void FrameTransformerTests::transformsGcrsToItrsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    compareVector(
        *result.vector,
        {
            .x = -0.1810332128305897282,
            .y = -0.9834768134136214897,
            .z = 0.5773474024748545878e-3,
        },
        2.0e-12
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(
            EphemerisCorrectionFlags::PrecessionNutation | EphemerisCorrectionFlags::EarthOrientation
        )
    );
}

void FrameTransformerTests::recordsPerStageMetadataForComposedTransforms()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(3U));
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[0].sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[0].targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages[0].metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::PrecessionNutation)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[1].sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[1].targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Tirs)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages[1].metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::EarthOrientation)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[2].sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Tirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[2].targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Itrs)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages[2].metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::EarthOrientation)
    );

    for (const CelestialFrameTransformStageMetadata& stage : result.stages) {
        QVERIFY(stage.applied);
        QCOMPARE(
            static_cast<std::uint8_t>(stage.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
        );
        QVERIFY(!stage.metadata.dataSourceProvenance.empty());
    }
}

void FrameTransformerTests::reusesTimeScaleConversionsAcrossComposedStages()
{
    auto service = frameTimeScaleService();
    const ErfaFrameTransformer transformer(service, earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch =
                {
                    .julianDatePart1 = 2'400'000.5,
                    .julianDatePart2 = 53'736.0,
                    .timeScale = TimeScale::Tai,
                },
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(service->convertCallCount, 2);
}

void FrameTransformerTests::reusesEarthOrientationSampleAcrossComposedTerrestrialStages()
{
    auto eopProvider = std::make_shared<RecordingEarthOrientationProvider>(earthOrientationProvider());
    const TimeScaleServiceOptions options{
        .allowDegradedLeapSecondFallback = false,
        .fallbackTaiMinusUtcSeconds = 0,
        .earthOrientationSampleOptions =
            {
                .allowOutOfRangeNearestSampleFallback = true,
                .allowMissingDataZeroFallback = false,
            },
        .allowUt1DeltaTFallback = false,
    };
    const auto service = std::make_shared<LeapSecondTimeScaleService>(leapSecondProvider(), options, eopProvider);
    const ErfaFrameTransformer transformer(service, eopProvider);

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(eopProvider->entriesCallCount, 2);
}

void FrameTransformerTests::preservesIcrsSourceFrameInComposedStageMetadata()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Icrs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(3U));
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Icrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
}

void FrameTransformerTests::preservesIcrsTargetFrameInComposedStageMetadata()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Itrs,
            .targetFrame = CelestialReferenceFrame::Icrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(3U));
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.back().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.back().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Icrs)
    );
}

void FrameTransformerTests::recordsUnavailableStageWhenTransformCannotBeComputed()
{
    const ErfaFrameTransformer transformer(nullptr);

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Cirs,
            .epoch = utcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(!result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(1U));
    QVERIFY(!result.stages.front().applied);
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().metadata.status),
        static_cast<std::uint8_t>(EphemerisResultStatus::Failed)
    );
    QVERIFY(result.stages.front().metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::NoCorrections)
    );
}

void FrameTransformerTests::acceptsItrsTransformWithPredictedEarthOrientationData()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider(true));

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Tirs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
    QVERIFY(!result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
}

void FrameTransformerTests::degradesItrsTransformForStaleEarthOrientationData()
{
    const ErfaFrameTransformer transformer(
        frameTimeScaleService(), earthOrientationProvider(false, EarthOrientationDataStatus::Stale)
    );

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Tirs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
}

void FrameTransformerTests::degradesItrsTransformForEstimatedEarthOrientationData()
{
    const ErfaFrameTransformer transformer(
        frameTimeScaleService(), earthOrientationProvider(false, EarthOrientationDataStatus::Estimated)
    );

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Tirs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
}

void FrameTransformerTests::degradesItrsTransformForMissingEarthOrientationData()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), nullptr);

    const CelestialFrameTransformResult result = transformer.transformCelestialVector(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Tirs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = sofaReferenceUtcEpoch(),
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        }
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
}

QTEST_MAIN(FrameTransformerTests)

#include "FrameTransformerTests.moc"
