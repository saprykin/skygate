#include "ErfaFrameTransformer.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "FrameTransformMatrixCache.hpp"
#include "FrameTransformStageCatalog.hpp"
#include "math/Matrix3x3.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr std::string_view kFrameTransformProvenance = "ERFA IAU 2006/2000A celestial and terrestrial frame transform";

class FrameTransformSession final {
public:
    FrameTransformSession(
        const CelestialReferenceFrame::Type sourceFrame,
        const CelestialReferenceFrame::Type targetFrame,
        const skygate::core::AstronomicalEpoch epoch,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider
    )
        : m_sourceFrame(sourceFrame), m_targetFrame(targetFrame),
          m_matrixCache(epoch, timeScaleService, earthOrientationProvider)
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

        CelestialFrameTransformResult result = makeValidResult();
        applyCatalogStages(vector, sourceRank, targetRank, result);
        return result;
    }

private:
    [[nodiscard]] static CelestialFrameTransformResult makeValidResult()
    {
        CelestialFrameTransformResult result;
        result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
        result.metadata.dataSourceProvenance = kFrameTransformProvenance;
        return result;
    }

    [[nodiscard]] static constexpr EphemerisMetadataMergeOptions stageMetadataMergeOptions() noexcept
    {
        EphemerisMetadataMergeOptions options = EphemerisMetadataMergeOptions::statusAndWarningsOnly();
        options.mergeCorrections = true;
        return options;
    }

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

    // Runs one stage of a composed transform: fetches the matrix the row names,
    // multiplies or transpose-multiplies 'vector' by it, and records the stage
    // with the correction it applied. A stage whose matrix is unavailable is
    // recorded too, and the transform stops with no vector.
    [[nodiscard]] std::optional<skygate::core::Vector3d> applyStage(
        const FrameTransformStageCatalog::Row& row,
        const CelestialReferenceFrame::Type sourceFrame,
        const CelestialReferenceFrame::Type targetFrame,
        const bool forward,
        const skygate::core::Vector3d& vector,
        CelestialFrameTransformResult& result
    )
    {
        CelestialFrameTransformResult::Stage stage = makeStage(sourceFrame, targetFrame);
        const std::optional<skygate::core::Matrix3x3> matrix = m_matrixCache.matrix(row.matrixKind, stage.metadata);
        if (!matrix.has_value()) {
            EphemerisMetadataMerger::markCorrectionUnavailable(stage.metadata, row.correction);
            appendStage(result, std::move(stage));
            return std::nullopt;
        }

        const skygate::core::Vector3d transformed =
            forward ? matrix->multiplied(vector) : matrix->transposeMultiplied(vector);
        stage.applied = true;
        EphemerisMetadataMerger::markCorrectionApplied(stage.metadata, row.correction);
        appendStage(result, std::move(stage));
        return transformed;
    }

    void applyCatalogStages(
        const skygate::core::Vector3d& vector,
        const std::uint8_t sourceRank,
        const std::uint8_t targetRank,
        CelestialFrameTransformResult& result
    )
    {
        const bool forward = sourceRank < targetRank;
        const std::uint8_t stageCount = forward ? static_cast<std::uint8_t>(targetRank - sourceRank)
                                                : static_cast<std::uint8_t>(sourceRank - targetRank);
        skygate::core::Vector3d transformed = vector;
        for (std::uint8_t index = 0U; index < stageCount; ++index) {
            const std::uint8_t lowerRank = forward ? static_cast<std::uint8_t>(sourceRank + index)
                                                   : static_cast<std::uint8_t>(sourceRank - 1U - index);
            const FrameTransformStageCatalog::Row& row = FrameTransformStageCatalog::rowForLowerRank(lowerRank);
            const bool firstStage = index == 0U;
            const bool lastStage = static_cast<std::uint8_t>(index + 1U) == stageCount;
            const std::optional<skygate::core::Vector3d> stageOutput = applyStage(
                row,
                firstStage ? m_sourceFrame : (forward ? row.sourceFrame : row.targetFrame),
                lastStage ? m_targetFrame : (forward ? row.targetFrame : row.sourceFrame),
                forward,
                transformed,
                result
            );
            if (!stageOutput.has_value()) {
                return;
            }

            transformed = *stageOutput;
        }

        result.vector = transformed;
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

        CelestialFrameTransformResult result = makeValidResult();
        result.vector = applyStage(
            FrameTransformStageCatalog::apparentEquatorAndEquinoxRow(),
            m_sourceFrame,
            m_targetFrame,
            forward,
            vector,
            result
        );
        return result;
    }

    const CelestialReferenceFrame::Type m_sourceFrame = CelestialReferenceFrame::Type::Gcrs;
    const CelestialReferenceFrame::Type m_targetFrame = CelestialReferenceFrame::Type::Cirs;
    FrameTransformMatrixCache m_matrixCache;
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
