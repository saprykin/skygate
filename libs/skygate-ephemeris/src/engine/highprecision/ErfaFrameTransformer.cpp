#include "ErfaFrameTransformer.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "ErfaAstrometry.hpp"
#include "math/MathConstants.hpp"
#include "math/Matrix3x3.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::MathConstants;

constexpr std::string_view kFrameTransformProvenance = "ERFA IAU 2006/2000A celestial and terrestrial frame transform";

class FrameTransformSession final {
public:
    FrameTransformSession(
        const CelestialReferenceFrame::Type sourceFrame,
        const CelestialReferenceFrame::Type targetFrame,
        AstronomicalEpoch epoch,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider
    )
        : m_sourceFrame(sourceFrame), m_targetFrame(targetFrame), m_epoch(epoch), m_timeScaleService(timeScaleService),
          m_earthOrientationProvider(earthOrientationProvider)
    {
    }

    [[nodiscard]] CelestialFrameTransformResult transformVector(const skygate::core::Vector3d& vector)
    {
        if (!vector.isFinite()) {
            return CelestialFrameTransformResult::makeFailed(
                EphemerisEngineWarning::Code::ComputationFailed, kFrameTransformProvenance
            );
        }

        const std::uint8_t sourceRank = CelestialReferenceFrame::rankFromType(m_sourceFrame);
        const std::uint8_t targetRank = CelestialReferenceFrame::rankFromType(m_targetFrame);
        if (sourceRank == targetRank
            && (m_sourceFrame == m_targetFrame
                || (CelestialReferenceFrame::isGcrsLike(m_sourceFrame)
                    && CelestialReferenceFrame::isGcrsLike(m_targetFrame)))) {
            return CelestialFrameTransformResult::makeIdentity(
                m_sourceFrame, m_targetFrame, vector, kFrameTransformProvenance
            );
        }

        CelestialFrameTransformResult result;
        result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
        result.metadata.dataSourceProvenance = kFrameTransformProvenance;

        if (std::optional<CelestialFrameTransformResult> apparentResult = tryTransformApparentEquatorAndEquinox(vector);
            apparentResult.has_value()) {
            return *apparentResult;
        }
        if (m_sourceFrame == CelestialReferenceFrame::Type::TrueEquatorAndEquinox
            || m_targetFrame == CelestialReferenceFrame::Type::TrueEquatorAndEquinox) {
            return CelestialFrameTransformResult::makeFailed(
                EphemerisEngineWarning::Code::CorrectionUnavailable, kFrameTransformProvenance
            );
        }
        if (sourceRank == targetRank) {
            return CelestialFrameTransformResult::makeFailed(
                EphemerisEngineWarning::Code::CorrectionUnavailable, kFrameTransformProvenance
            );
        }

        skygate::core::Vector3d transformed = vector;
        const std::vector<StagePlan> stagePlan = buildTransformStagePlan(sourceRank, targetRank);
        for (const StagePlan& plannedStage : stagePlan) {
            CelestialFrameTransformResult::Stage stage = makeStage(plannedStage.sourceFrame, plannedStage.targetFrame);
            const std::optional<skygate::core::Matrix3x3> matrix = stageMatrix(plannedStage.lowerRank, stage.metadata);
            if (!matrix.has_value()) {
                markStageUnavailable(stage, plannedStage.correction);
                appendStage(result, std::move(stage));
                return result;
            }

            transformed =
                plannedStage.forward ? matrix->multiplied(transformed) : matrix->transposeMultiplied(transformed);
            stage.applied = true;
            EphemerisMetadataMerger::markCorrectionApplied(stage.metadata, plannedStage.correction);
            appendStage(result, std::move(stage));
        }

        result.vector = transformed;
        return result;
    }

private:
    [[nodiscard]] static constexpr EphemerisMetadataMergeOptions cachedMetadataMergeOptions() noexcept
    {
        return EphemerisMetadataMergeOptions{
            .statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly,
            .mergeCorrections = false,
            .mergeProvenance = false,
            .mergeValidityRange = false,
            .mergeAngularUncertainty = false,
        };
    }

    [[nodiscard]] static constexpr EphemerisMetadataMergeOptions stageMetadataMergeOptions() noexcept
    {
        return EphemerisMetadataMergeOptions{
            .statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly,
            .mergeCorrections = true,
            .mergeProvenance = false,
            .mergeValidityRange = false,
            .mergeAngularUncertainty = false,
        };
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags
    correctionForLowerRank(const std::uint8_t lowerRank) noexcept
    {
        switch (lowerRank) {
        case 0U:
            return EphemerisCorrectionFlags::precessionNutation();
        case 1U:
            return EphemerisCorrectionFlags::earthOrientation();
        case 2U:
            return EphemerisCorrectionFlags::earthOrientation();
        default:
            return EphemerisCorrectionFlags::noCorrections();
        }
    }

    struct MatrixCacheEntry {
        std::optional<skygate::core::Matrix3x3> matrix;
        EphemerisEngineQueryResult metadata;

        void mergeMetadataInto(EphemerisEngineQueryResult& target) const noexcept
        {
            EphemerisMetadataMerger::merge(target, metadata, FrameTransformSession::cachedMetadataMergeOptions());
        }
    };

    struct StagePlan {
        CelestialReferenceFrame::Type sourceFrame = CelestialReferenceFrame::Type::Gcrs;
        CelestialReferenceFrame::Type targetFrame = CelestialReferenceFrame::Type::Cirs;
        std::uint8_t lowerRank = 0U;
        EphemerisCorrectionFlags correction = EphemerisCorrectionFlags::noCorrections();
        bool forward = true;
    };

    [[nodiscard]] static CelestialFrameTransformResult::Stage
    makeStage(const CelestialReferenceFrame::Type sourceFrame, const CelestialReferenceFrame::Type targetFrame)
    {
        CelestialFrameTransformResult::Stage stage;
        stage.sourceFrame = sourceFrame;
        stage.targetFrame = targetFrame;
        stage.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        stage.metadata.dataSourceProvenance = kFrameTransformProvenance;
        return stage;
    }

    static void appendStage(CelestialFrameTransformResult& result, CelestialFrameTransformResult::Stage stage)
    {
        EphemerisMetadataMerger::merge(result.metadata, stage.metadata, stageMetadataMergeOptions());
        result.stages.push_back(std::move(stage));
    }

    static void markStageUnavailable(
        CelestialFrameTransformResult::Stage& stage, const EphemerisCorrectionFlags unavailableCorrection
    ) noexcept
    {
        EphemerisMetadataMerger::markCorrectionUnavailable(stage.metadata, unavailableCorrection);
    }

    [[nodiscard]] std::optional<AstronomicalEpoch>
    epochInScale(TimeScale targetScale, EphemerisEngineQueryResult& metadata)
    {
        if (!m_epoch.isFinite()) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            return std::nullopt;
        }

        if (m_epoch.timeScale == targetScale) {
            return m_epoch.normalized();
        }

        if (m_timeScaleService == nullptr) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            return std::nullopt;
        }

        std::optional<TimeScaleConversionResult>* cachedConversion = conversionCacheFor(targetScale);
        if (cachedConversion == nullptr) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            return std::nullopt;
        }
        if (!cachedConversion->has_value()) {
            *cachedConversion = m_timeScaleService->convert(m_epoch, targetScale);
        }

        const TimeScaleConversionResult& conversion = **cachedConversion;
        EphemerisMetadataMerger::mergeTimeScale(metadata, conversion, EphemerisMetadataFailurePolicy::MarkFailed);
        if (!conversion.isSuccess()) {
            return std::nullopt;
        }

        return conversion.epoch;
    }

    [[nodiscard]] std::optional<EarthOrientationSample> earthOrientation(EphemerisEngineQueryResult& metadata)
    {
        const std::optional<AstronomicalEpoch> utcEpoch = epochInScale(TimeScale::Utc, metadata);
        if (!utcEpoch.has_value()) {
            return std::nullopt;
        }

        if (!m_earthOrientationSample.has_value()) {
            m_earthOrientationSample = sampleEarthOrientation(
                m_earthOrientationProvider,
                *utcEpoch,
                EarthOrientationSampleOptions{
                    .allowOutOfRangeNearestSampleFallback = true,
                    .allowMissingDataZeroFallback = true,
                    .degradePredictedData = false,
                }
            );
        }

        EphemerisMetadataMerger::mergeEarthOrientation(metadata, *m_earthOrientationSample);
        if (!m_earthOrientationSample->isSuccess()) {
            return std::nullopt;
        }

        return m_earthOrientationSample;
    }

    [[nodiscard]] std::optional<AstronomicalEpoch> ut1Epoch(EphemerisEngineQueryResult& metadata)
    {
        if (!m_epoch.isFinite()) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            return std::nullopt;
        }

        if (m_epoch.timeScale == TimeScale::Ut1 || m_earthOrientationProvider == nullptr) {
            return epochInScale(TimeScale::Ut1, metadata);
        }

        const std::optional<EarthOrientationSample> sample = earthOrientation(metadata);
        if (!sample.has_value()) {
            return std::nullopt;
        }

        return sample->requestedUtcEpoch.addSeconds(sample->ut1MinusUtcSeconds, TimeScale::Ut1);
    }

    [[nodiscard]] std::optional<skygate::core::Matrix3x3>
    celestialIntermediateMatrix(EphemerisEngineQueryResult& metadata)
    {
        if (!m_celestialIntermediateMatrix.has_value()) {
            MatrixCacheEntry cacheEntry;
            const std::optional<AstronomicalEpoch> ttEpoch = epochInScale(TimeScale::Tt, cacheEntry.metadata);
            if (ttEpoch.has_value()) {
                cacheEntry.matrix = ErfaAstrometry::celestialToIntermediateMatrix06A(*ttEpoch);
                if (!cacheEntry.matrix.has_value()) {
                    cacheEntry.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
                    cacheEntry.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                }
            }
            m_celestialIntermediateMatrix = cacheEntry;
        }

        m_celestialIntermediateMatrix->mergeMetadataInto(metadata);
        return m_celestialIntermediateMatrix->matrix;
    }

    [[nodiscard]] std::optional<skygate::core::Matrix3x3>
    intermediateToTerrestrialIntermediateMatrix(EphemerisEngineQueryResult& metadata)
    {
        if (!m_earthRotationMatrix.has_value()) {
            MatrixCacheEntry cacheEntry;
            const std::optional<AstronomicalEpoch> universalTime1 = ut1Epoch(cacheEntry.metadata);
            if (universalTime1.has_value()) {
                cacheEntry.matrix = ErfaAstrometry::earthRotationMatrix00(*universalTime1);
                if (!cacheEntry.matrix.has_value()) {
                    cacheEntry.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
                    cacheEntry.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                }
            }
            m_earthRotationMatrix = cacheEntry;
        }

        m_earthRotationMatrix->mergeMetadataInto(metadata);
        return m_earthRotationMatrix->matrix;
    }

    [[nodiscard]] std::optional<skygate::core::Matrix3x3>
    terrestrialIntermediateToTerrestrialMatrix(EphemerisEngineQueryResult& metadata)
    {
        if (!m_polarMotionMatrix.has_value()) {
            MatrixCacheEntry cacheEntry;
            const std::optional<AstronomicalEpoch> ttEpoch = epochInScale(TimeScale::Tt, cacheEntry.metadata);
            const std::optional<EarthOrientationSample> earthOrientationSample = earthOrientation(cacheEntry.metadata);
            if (ttEpoch.has_value() && earthOrientationSample.has_value()) {
                const std::optional<double> tioLocator = ErfaAstrometry::tioLocatorS00(*ttEpoch);
                if (tioLocator.has_value()) {
                    cacheEntry.matrix = ErfaAstrometry::polarMotionMatrix00(
                        earthOrientationSample->polarMotionXArcseconds * MathConstants::kArcsecondsToRadians,
                        earthOrientationSample->polarMotionYArcseconds * MathConstants::kArcsecondsToRadians,
                        *tioLocator
                    );
                    if (!cacheEntry.matrix.has_value()) {
                        cacheEntry.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
                        cacheEntry.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                    }
                } else {
                    cacheEntry.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
                    cacheEntry.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                }
            }
            m_polarMotionMatrix = cacheEntry;
        }

        m_polarMotionMatrix->mergeMetadataInto(metadata);
        return m_polarMotionMatrix->matrix;
    }

    [[nodiscard]] std::optional<skygate::core::Matrix3x3>
    apparentEquatorAndEquinoxMatrix(EphemerisEngineQueryResult& metadata)
    {
        if (!m_apparentEquatorAndEquinoxMatrix.has_value()) {
            MatrixCacheEntry cacheEntry;
            const std::optional<AstronomicalEpoch> ttEpoch = epochInScale(TimeScale::Tt, cacheEntry.metadata);
            if (ttEpoch.has_value()) {
                cacheEntry.matrix = ErfaAstrometry::precessionNutationMatrix06A(*ttEpoch);
                if (!cacheEntry.matrix.has_value()) {
                    cacheEntry.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
                    cacheEntry.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                }
            }
            m_apparentEquatorAndEquinoxMatrix = cacheEntry;
        }

        m_apparentEquatorAndEquinoxMatrix->mergeMetadataInto(metadata);
        return m_apparentEquatorAndEquinoxMatrix->matrix;
    }

    [[nodiscard]] std::optional<skygate::core::Matrix3x3>
    stageMatrix(const std::uint8_t lowerRank, EphemerisEngineQueryResult& metadata)
    {
        switch (lowerRank) {
        case 0U:
            return celestialIntermediateMatrix(metadata);
        case 1U:
            return intermediateToTerrestrialIntermediateMatrix(metadata);
        case 2U:
            return terrestrialIntermediateToTerrestrialMatrix(metadata);
        default:
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            return std::nullopt;
        }
    }

    [[nodiscard]] std::vector<StagePlan>
    buildTransformStagePlan(const std::uint8_t sourceRank, const std::uint8_t targetRank) const
    {
        std::vector<StagePlan> stages;
        if (sourceRank < targetRank) {
            stages.reserve(targetRank - sourceRank);
            for (std::uint8_t lowerRank = sourceRank; lowerRank < targetRank; ++lowerRank) {
                const std::uint8_t upperRank = static_cast<std::uint8_t>(lowerRank + 1U);
                stages.push_back(
                    StagePlan{
                        .sourceFrame =
                            lowerRank == sourceRank ? m_sourceFrame : CelestialReferenceFrame::typeFromRank(lowerRank),
                        .targetFrame =
                            upperRank == targetRank ? m_targetFrame : CelestialReferenceFrame::typeFromRank(upperRank),
                        .lowerRank = lowerRank,
                        .correction = correctionForLowerRank(lowerRank),
                        .forward = true,
                    }
                );
            }
            return stages;
        }

        stages.reserve(sourceRank - targetRank);
        for (std::uint8_t lowerRank = sourceRank; lowerRank > targetRank; --lowerRank) {
            const std::uint8_t stageLowerRank = static_cast<std::uint8_t>(lowerRank - 1U);
            stages.push_back(
                StagePlan{
                    .sourceFrame =
                        lowerRank == sourceRank ? m_sourceFrame : CelestialReferenceFrame::typeFromRank(lowerRank),
                    .targetFrame = stageLowerRank == targetRank ? m_targetFrame
                                                                : CelestialReferenceFrame::typeFromRank(stageLowerRank),
                    .lowerRank = stageLowerRank,
                    .correction = correctionForLowerRank(stageLowerRank),
                    .forward = false,
                }
            );
        }
        return stages;
    }

    [[nodiscard]] std::optional<CelestialFrameTransformResult>
    tryTransformApparentEquatorAndEquinox(const skygate::core::Vector3d& vector)
    {
        const bool forward = CelestialReferenceFrame::isGcrsLike(m_sourceFrame)
                             && m_targetFrame == CelestialReferenceFrame::Type::TrueEquatorAndEquinox;
        const bool reverse = m_sourceFrame == CelestialReferenceFrame::Type::TrueEquatorAndEquinox
                             && CelestialReferenceFrame::isGcrsLike(m_targetFrame);
        if (!forward && !reverse) {
            return std::nullopt;
        }

        CelestialFrameTransformResult result;
        result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
        result.metadata.dataSourceProvenance = kFrameTransformProvenance;

        CelestialFrameTransformResult::Stage stage = makeStage(m_sourceFrame, m_targetFrame);
        const std::optional<skygate::core::Matrix3x3> matrix = apparentEquatorAndEquinoxMatrix(stage.metadata);
        if (!matrix.has_value()) {
            markStageUnavailable(stage, EphemerisCorrectionFlags::precessionNutation());
            appendStage(result, std::move(stage));
            return result;
        }

        result.vector = forward ? matrix->multiplied(vector) : matrix->transposeMultiplied(vector);
        stage.applied = true;
        EphemerisMetadataMerger::markCorrectionApplied(stage.metadata, EphemerisCorrectionFlags::precessionNutation());
        appendStage(result, std::move(stage));
        return result;
    }

    [[nodiscard]] std::optional<TimeScaleConversionResult>* conversionCacheFor(const TimeScale targetScale)
    {
        switch (targetScale) {
        case TimeScale::Tt:
            return &m_ttConversion;
        case TimeScale::Utc:
            return &m_utcConversion;
        case TimeScale::Ut1:
            return &m_ut1Conversion;
        case TimeScale::Tai:
        case TimeScale::Tdb:
            return nullptr;
        }

        return nullptr;
    }

    const CelestialReferenceFrame::Type m_sourceFrame = CelestialReferenceFrame::Type::Gcrs;
    const CelestialReferenceFrame::Type m_targetFrame = CelestialReferenceFrame::Type::Cirs;
    const AstronomicalEpoch m_epoch;
    const skygate::ephemeris::ITimeScaleService* const m_timeScaleService = nullptr;
    const skygate::ephemeris::IEarthOrientationProvider* const m_earthOrientationProvider = nullptr;
    std::optional<TimeScaleConversionResult> m_ttConversion;
    std::optional<TimeScaleConversionResult> m_utcConversion;
    std::optional<TimeScaleConversionResult> m_ut1Conversion;
    std::optional<EarthOrientationSample> m_earthOrientationSample;
    std::optional<MatrixCacheEntry> m_celestialIntermediateMatrix;
    std::optional<MatrixCacheEntry> m_earthRotationMatrix;
    std::optional<MatrixCacheEntry> m_polarMotionMatrix;
    std::optional<MatrixCacheEntry> m_apparentEquatorAndEquinoxMatrix;
};

}  // namespace

ErfaFrameTransformer::ErfaFrameTransformer(
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider
)
    : m_timeScaleService(std::move(timeScaleService)), m_earthOrientationProvider(std::move(earthOrientationProvider))
{
}

std::vector<CelestialFrameTransformResult>
ErfaFrameTransformer::transform(const CelestialFrameTransformRequest& request) const
{
    std::vector<CelestialFrameTransformResult> results;
    results.reserve(request.vectors.size());
    if (request.vectors.empty()) {
        return results;
    }

    FrameTransformSession session{
        request.sourceFrame,
        request.targetFrame,
        request.epoch,
        m_timeScaleService.get(),
        m_earthOrientationProvider.get(),
    };
    for (const skygate::core::Vector3d& vector : request.vectors) {
        results.push_back(session.transformVector(vector));
    }

    return results;
}

}  // namespace skygate::ephemeris::highprecision
