#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/LeapSecondProvider.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

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
using namespace skygate::core;

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

void compareVector(const Vector3d& actual, const Vector3d& expected, const double tolerance)
{
    QVERIFY(std::abs(actual.x - expected.x) <= tolerance);
    QVERIFY(std::abs(actual.y - expected.y) <= tolerance);
    QVERIFY(std::abs(actual.z - expected.z) <= tolerance);
}

[[nodiscard]] CelestialFrameTransformResult transformSingleVector(
    const IFrameTransformer& transformer,
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const AstronomicalEpoch& epoch,
    const Vector3d& vector
)
{
    const std::array<Vector3d, 1U> vectors{vector};
    const std::vector<CelestialFrameTransformResult> results = transformer.transform(
        CelestialFrameTransformRequest{
            .sourceFrame = sourceFrame,
            .targetFrame = targetFrame,
            .epoch = epoch,
            .vectors = vectors,
        }
    );
    return results.front();
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

class ErfaFrameTransformerTests final : public QObject {
    Q_OBJECT

private slots:
    void transformsGcrsToCirsAgainstSofaReference();
    void roundTripsGcrsAndCirsVectors();
    void treatsIcrsAndGcrsAsIdentityCelestialAxes();
    void returnsEmptyResultsForEmptyVectorBatch();
    void preservesBatchResultOrder();
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

void ErfaFrameTransformerTests::transformsGcrsToCirsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Cirs,
        sofaReferenceTtEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    compareVector(
        *result.vector,
        {
            .x = 0.99999983230371603,
            .y = -0.23842531691809769e-7,
            .z = 0.57913084828352932e-3,
        },
        kTolerance
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::precessionNutation())
    );
}

void ErfaFrameTransformerTests::roundTripsGcrsAndCirsVectors()
{
    const ErfaFrameTransformer transformer(nullptr);
    const Vector3d gcrsVector{.x = 0.4, .y = -0.2, .z = 0.9};

    const CelestialFrameTransformResult cirs = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Cirs,
        sofaReferenceTtEpoch(),
        gcrsVector
    );
    QVERIFY(cirs.vector.has_value());

    const CelestialFrameTransformResult gcrs = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Cirs,
        CelestialReferenceFrame::Type::Gcrs,
        sofaReferenceTtEpoch(),
        *cirs.vector
    );

    QVERIFY(gcrs.vector.has_value());
    compareVector(*gcrs.vector, gcrsVector, 2.0e-14);
}

void ErfaFrameTransformerTests::treatsIcrsAndGcrsAsIdentityCelestialAxes()
{
    const ErfaFrameTransformer transformer(nullptr);
    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Icrs,
        CelestialReferenceFrame::Type::Gcrs,
        {
            .julianDatePart1 = std::numeric_limits<double>::quiet_NaN(),
            .julianDatePart2 = std::numeric_limits<double>::quiet_NaN(),
            .timeScale = TimeScale::Utc,
        },
        {.x = 0.25, .y = 0.5, .z = -0.75}
    );

    QVERIFY(result.vector.has_value());
    compareVector(*result.vector, {.x = 0.25, .y = 0.5, .z = -0.75}, 0.0);
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::noCorrections())
    );
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(1U));
    QVERIFY(!result.stages.front().applied);
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Icrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages.front().metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::noCorrections())
    );
    QVERIFY(!result.stages.front().metadata.dataSourceProvenance.empty());
}

void ErfaFrameTransformerTests::returnsEmptyResultsForEmptyVectorBatch()
{
    const ErfaFrameTransformer transformer(nullptr);

    const std::vector<CelestialFrameTransformResult> results = transformer.transform(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
            .targetFrame = CelestialReferenceFrame::Type::Cirs,
            .epoch = sofaReferenceTtEpoch(),
            .vectors = {},
        }
    );

    QVERIFY(results.empty());
}

void ErfaFrameTransformerTests::preservesBatchResultOrder()
{
    const ErfaFrameTransformer transformer(nullptr);
    const std::array<Vector3d, 3U> vectors{{
        {.x = 1.0, .y = 0.0, .z = 0.0},
        {.x = 0.0, .y = 1.0, .z = 0.0},
        {.x = 0.0, .y = 0.0, .z = 1.0},
    }};

    const std::vector<CelestialFrameTransformResult> results = transformer.transform(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Type::Icrs,
            .targetFrame = CelestialReferenceFrame::Type::Gcrs,
            .epoch =
                {
                    .julianDatePart1 = std::numeric_limits<double>::quiet_NaN(),
                    .julianDatePart2 = std::numeric_limits<double>::quiet_NaN(),
                    .timeScale = TimeScale::Utc,
                },
            .vectors = vectors,
        }
    );

    QCOMPARE(results.size(), vectors.size());
    for (std::size_t index = 0U; index < vectors.size(); ++index) {
        QVERIFY(results[index].vector.has_value());
        compareVector(*results[index].vector, vectors[index], 0.0);
    }
}

void ErfaFrameTransformerTests::usesTimeScaleServiceForNonTtEpochs()
{
    auto service = std::make_shared<RecordingTimeScaleService>();
    service->nextResult = validTtConversionResult();
    const ErfaFrameTransformer transformer(service);
    const AstronomicalEpoch epoch = utcEpoch();

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Cirs,
        epoch,
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(service->convertCallCount, 1);
    QCOMPARE(static_cast<std::uint8_t>(service->lastTargetScale), static_cast<std::uint8_t>(TimeScale::Tt));
    QCOMPARE(service->lastEpoch.julianDatePart1, epoch.julianDatePart1);
    QCOMPARE(service->lastEpoch.julianDatePart2, epoch.julianDatePart2);
}

void ErfaFrameTransformerTests::reportsMissingTimeScaleServiceForNonTtCirsTransforms()
{
    const ErfaFrameTransformer transformer(nullptr);

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Cirs,
        utcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(!result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
}

void ErfaFrameTransformerTests::rejectsUnsupportedTrueEquatorAndEquinoxTerrestrialTransforms()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());
    const std::array<std::pair<CelestialReferenceFrame::Type, CelestialReferenceFrame::Type>, 4U> cases{{
        {CelestialReferenceFrame::Type::TrueEquatorAndEquinox, CelestialReferenceFrame::Type::Tirs},
        {CelestialReferenceFrame::Type::Tirs, CelestialReferenceFrame::Type::TrueEquatorAndEquinox},
        {CelestialReferenceFrame::Type::TrueEquatorAndEquinox, CelestialReferenceFrame::Type::Itrs},
        {CelestialReferenceFrame::Type::Itrs, CelestialReferenceFrame::Type::TrueEquatorAndEquinox},
    }};

    for (const auto& [sourceFrame, targetFrame] : cases) {
        const CelestialFrameTransformResult result = transformSingleVector(
            transformer, sourceFrame, targetFrame, sofaReferenceUtcEpoch(), {.x = 1.0, .y = 0.0, .z = 0.0}
        );

        QVERIFY(!result.vector.has_value());
        QCOMPARE(
            static_cast<std::uint8_t>(result.metadata.status),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
        );
        QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
        QCOMPARE(
            static_cast<std::uint32_t>(result.metadata.appliedCorrections),
            static_cast<std::uint32_t>(EphemerisCorrectionFlags::noCorrections())
        );
        QCOMPARE(result.stages.size(), static_cast<std::size_t>(0U));
    }
}

void ErfaFrameTransformerTests::transformsCirsToTirsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Cirs,
        CelestialReferenceFrame::Type::Tirs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
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
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::earthOrientation())
    );
}

void ErfaFrameTransformerTests::transformsTirsToItrsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Tirs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
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

void ErfaFrameTransformerTests::transformsGcrsToItrsAgainstSofaReference()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
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
            EphemerisCorrectionFlags::precessionNutation() | EphemerisCorrectionFlags::earthOrientation()
        )
    );
}

void ErfaFrameTransformerTests::recordsPerStageMetadataForComposedTransforms()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(3U));
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[0].sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[0].targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages[0].metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::precessionNutation())
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[1].sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[1].targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Tirs)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages[1].metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::earthOrientation())
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[2].sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Tirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages[2].targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Itrs)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.stages[2].metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::earthOrientation())
    );

    for (const CelestialFrameTransformResult::Stage& stage : result.stages) {
        QVERIFY(stage.applied);
        QCOMPARE(
            static_cast<std::uint8_t>(stage.metadata.status),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
        );
        QVERIFY(!stage.metadata.dataSourceProvenance.empty());
    }
}

void ErfaFrameTransformerTests::reusesTimeScaleConversionsAcrossComposedStages()
{
    auto service = frameTimeScaleService();
    const ErfaFrameTransformer transformer(service, earthOrientationProvider());
    const std::array<Vector3d, 3U> vectors{{
        {.x = 1.0, .y = 0.0, .z = 0.0},
        {.x = 0.0, .y = 1.0, .z = 0.0},
        {.x = 0.0, .y = 0.0, .z = 1.0},
    }};

    const std::vector<CelestialFrameTransformResult> results = transformer.transform(
        CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
            .targetFrame = CelestialReferenceFrame::Type::Itrs,
            .epoch =
                {
                    .julianDatePart1 = 2'400'000.5,
                    .julianDatePart2 = 53'736.0,
                    .timeScale = TimeScale::Tai,
                },
            .vectors = vectors,
        }
    );

    QCOMPARE(results.size(), vectors.size());
    for (const CelestialFrameTransformResult& result : results) {
        QVERIFY(result.vector.has_value());
    }
    QCOMPARE(service->convertCallCount, 2);
}

void ErfaFrameTransformerTests::reusesEarthOrientationSampleAcrossComposedTerrestrialStages()
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

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(eopProvider->entriesCallCount, 2);
}

void ErfaFrameTransformerTests::preservesIcrsSourceFrameInComposedStageMetadata()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Icrs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(3U));
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Icrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Cirs)
    );
}

void ErfaFrameTransformerTests::preservesIcrsTargetFrameInComposedStageMetadata()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider());

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Itrs,
        CelestialReferenceFrame::Type::Icrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(3U));
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.back().sourceFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Cirs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.back().targetFrame),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Type::Icrs)
    );
}

void ErfaFrameTransformerTests::recordsUnavailableStageWhenTransformCannotBeComputed()
{
    const ErfaFrameTransformer transformer(nullptr);

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Cirs,
        utcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(!result.vector.has_value());
    QCOMPARE(result.stages.size(), static_cast<std::size_t>(1U));
    QVERIFY(!result.stages.front().applied);
    QCOMPARE(
        static_cast<std::uint8_t>(result.stages.front().metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(result.stages.front().metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(result.stages.front().metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::noCorrections())
    );
    QCOMPARE(
        static_cast<std::uint32_t>(result.metadata.unavailableCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::precessionNutation())
    );
}

void ErfaFrameTransformerTests::acceptsItrsTransformWithPredictedEarthOrientationData()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), earthOrientationProvider(true));

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Tirs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(!result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
}

void ErfaFrameTransformerTests::degradesItrsTransformForStaleEarthOrientationData()
{
    const ErfaFrameTransformer transformer(
        frameTimeScaleService(), earthOrientationProvider(false, EarthOrientationDataStatus::Stale)
    );

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Tirs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
}

void ErfaFrameTransformerTests::degradesItrsTransformForEstimatedEarthOrientationData()
{
    const ErfaFrameTransformer transformer(
        frameTimeScaleService(), earthOrientationProvider(false, EarthOrientationDataStatus::Estimated)
    );

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Tirs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
}

void ErfaFrameTransformerTests::degradesItrsTransformForMissingEarthOrientationData()
{
    const ErfaFrameTransformer transformer(frameTimeScaleService(), nullptr);

    const CelestialFrameTransformResult result = transformSingleVector(
        transformer,
        CelestialReferenceFrame::Type::Tirs,
        CelestialReferenceFrame::Type::Itrs,
        sofaReferenceUtcEpoch(),
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );

    QVERIFY(result.vector.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(!result.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
}

QTEST_MAIN(ErfaFrameTransformerTests)

#include "ErfaFrameTransformerTests.moc"
