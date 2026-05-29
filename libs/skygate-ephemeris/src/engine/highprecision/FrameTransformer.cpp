#include "FrameTransformer.hpp"
#include "EphemerisMetadataMerge.hpp"
#include "ErfaAstrometry.hpp"
#include "math/MathConstants.hpp"
#include "math/TimeConstants.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::MathConstants;
using skygate::core::TimeConstants;

constexpr std::string_view kFrameTransformProvenance = "ERFA IAU 2006/2000A celestial and terrestrial frame transform";

[[nodiscard]] bool isFiniteVector(const skygate::core::Vector3d& vector) noexcept
{
    return vector.isFinite();
}

[[nodiscard]] std::uint8_t frameRank(const CelestialReferenceFrame frame) noexcept
{
    switch (frame) {
    case CelestialReferenceFrame::Icrs:
    case CelestialReferenceFrame::Gcrs:
        return 0U;
    case CelestialReferenceFrame::TrueEquatorAndEquinox:
    case CelestialReferenceFrame::Cirs:
        return 1U;
    case CelestialReferenceFrame::Tirs:
        return 2U;
    case CelestialReferenceFrame::Itrs:
        return 3U;
    }

    return 0U;
}

[[nodiscard]] CelestialReferenceFrame frameForRank(const std::uint8_t rank) noexcept
{
    switch (rank) {
    case 0U:
        return CelestialReferenceFrame::Gcrs;
    case 1U:
        return CelestialReferenceFrame::Cirs;
    case 2U:
        return CelestialReferenceFrame::Tirs;
    case 3U:
        return CelestialReferenceFrame::Itrs;
    default:
        return CelestialReferenceFrame::Gcrs;
    }
}

[[nodiscard]] CelestialFrameTransformResult makeFailedResult(const EphemerisEngineWarning::Code warningCode)
{
    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;
    return result;
}

[[nodiscard]] CelestialFrameTransformResult makeIdentityResult(const CelestialFrameTransformRequest& request)
{
    CelestialFrameTransformResult result;
    result.vector = request.vector;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;
    if (request.sourceFrame != request.targetFrame) {
        CelestialFrameTransformStageMetadata stage;
        stage.sourceFrame = request.sourceFrame;
        stage.targetFrame = request.targetFrame;
        stage.applied = false;
        stage.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        stage.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
        stage.metadata.dataSourceProvenance = kFrameTransformProvenance;
        result.stages.push_back(std::move(stage));
    }
    return result;
}

[[nodiscard]] skygate::core::Vector3d multiply(const Matrix3x3& matrix, const skygate::core::Vector3d& vector) noexcept
{
    return {
        .x = matrix[0][0] * vector.x + matrix[0][1] * vector.y + matrix[0][2] * vector.z,
        .y = matrix[1][0] * vector.x + matrix[1][1] * vector.y + matrix[1][2] * vector.z,
        .z = matrix[2][0] * vector.x + matrix[2][1] * vector.y + matrix[2][2] * vector.z,
    };
}

[[nodiscard]] skygate::core::Vector3d
multiplyTranspose(const Matrix3x3& matrix, const skygate::core::Vector3d& vector) noexcept
{
    return {
        .x = matrix[0][0] * vector.x + matrix[1][0] * vector.y + matrix[2][0] * vector.z,
        .y = matrix[0][1] * vector.x + matrix[1][1] * vector.y + matrix[2][1] * vector.z,
        .z = matrix[0][2] * vector.x + matrix[1][2] * vector.y + matrix[2][2] * vector.z,
    };
}

[[nodiscard]] AstronomicalEpoch
addSeconds(const AstronomicalEpoch& epoch, const double seconds, const TimeScale targetScale) noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = epoch.julianDatePart1,
        .julianDatePart2 = epoch.julianDatePart2 + seconds / TimeConstants::kSecondsPerDay,
        .timeScale = targetScale,
    }
        .normalized();
}

[[nodiscard]] Matrix3x3 earthRotationMatrix(const double earthRotationAngle) noexcept
{
    const double sine = std::sin(earthRotationAngle);
    const double cosine = std::cos(earthRotationAngle);
    return Matrix3x3{
        std::array<double, 3>{cosine, sine, 0.0},
        std::array<double, 3>{-sine, cosine, 0.0},
        std::array<double, 3>{0.0, 0.0, 1.0},
    };
}

void addDegradedWarning(EphemerisEngineQueryResult& metadata, const EphemerisEngineWarning::Code warningCode) noexcept
{
    if (metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
        metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
    metadata.addWarning(warningCode);
}

void mergeTimeScaleWarnings(EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion) noexcept
{
    if (conversion.status != TimeScaleConversionStatus::Degraded) {
        return;
    }

    addDegradedWarning(metadata, EphemerisEngineWarning::Code::AccuracyDegraded);
    if (conversion.hasWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::EarthOrientationDataMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::DeltaTUnavailable)) {
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    }
    if (conversion.hasWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable)
        || conversion.hasWarning(TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData)) {
        metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    }
}

void mergeEarthOrientationWarnings(EphemerisEngineQueryResult& metadata, const EarthOrientationSample& sample) noexcept
{
    if (sample.status != EarthOrientationSampleStatus::Degraded) {
        return;
    }

    addDegradedWarning(metadata, EphemerisEngineWarning::Code::AccuracyDegraded);
    if (sample.hasWarning(EarthOrientationSampleWarningCode::MissingData)) {
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::EpochOutsideRange)) {
        metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    }
}

void mergeCachedTransformMetadata(EphemerisEngineQueryResult& target, const EphemerisEngineQueryResult& source) noexcept
{
    EphemerisMetadataMerger::merge(
        target,
        source,
        EphemerisMetadataMergeOptions{
            .statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly,
            .mergeCorrections = false,
            .mergeProvenance = false,
            .mergeValidityRange = false,
            .mergeAngularUncertainty = false,
        }
    );
}

struct FrameTransformContext {
    const CelestialFrameTransformRequest& request;
    const skygate::ephemeris::ITimeScaleService* timeScaleService = nullptr;
    const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider = nullptr;
    mutable std::optional<TimeScaleConversionResult> ttConversion;
    mutable std::optional<TimeScaleConversionResult> utcConversion;
    mutable std::optional<TimeScaleConversionResult> ut1Conversion;
    mutable bool earthOrientationSampleComputed = false;
    mutable EarthOrientationSample earthOrientationSample;
    mutable bool celestialIntermediateMatrixComputed = false;
    mutable std::optional<Matrix3x3> celestialIntermediateMatrixValue;
    mutable EphemerisEngineQueryResult celestialIntermediateMatrixMetadata;
    mutable bool earthRotationMatrixComputed = false;
    mutable std::optional<Matrix3x3> earthRotationMatrixValue;
    mutable EphemerisEngineQueryResult earthRotationMatrixMetadata;
    mutable bool polarMotionMatrixComputed = false;
    mutable std::optional<Matrix3x3> polarMotionMatrixValue;
    mutable EphemerisEngineQueryResult polarMotionMatrixMetadata;
    mutable bool apparentEquatorAndEquinoxMatrixComputed = false;
    mutable std::optional<Matrix3x3> apparentEquatorAndEquinoxMatrixValue;
    mutable EphemerisEngineQueryResult apparentEquatorAndEquinoxMatrixMetadata;

    [[nodiscard]] std::optional<AstronomicalEpoch>
    epochInScale(TimeScale targetScale, EphemerisEngineQueryResult& metadata) const
    {
        if (!request.epoch.isFinite()) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            return std::nullopt;
        }

        if (request.epoch.timeScale == targetScale) {
            return request.epoch.normalized();
        }

        if (timeScaleService == nullptr) {
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
            *cachedConversion = timeScaleService->convert(request.epoch, targetScale);
        }

        const TimeScaleConversionResult& conversion = **cachedConversion;
        if (!conversion.isSuccess()) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            return std::nullopt;
        }

        mergeTimeScaleWarnings(metadata, conversion);
        return conversion.epoch;
    }

    [[nodiscard]] std::optional<EarthOrientationSample> earthOrientation(EphemerisEngineQueryResult& metadata) const
    {
        const std::optional<AstronomicalEpoch> utcEpoch = epochInScale(TimeScale::Utc, metadata);
        if (!utcEpoch.has_value()) {
            return std::nullopt;
        }

        if (!earthOrientationSampleComputed) {
            earthOrientationSample = sampleEarthOrientation(
                earthOrientationProvider,
                *utcEpoch,
                EarthOrientationSampleOptions{
                    .allowOutOfRangeNearestSampleFallback = true,
                    .allowMissingDataZeroFallback = true,
                    .degradePredictedData = false,
                }
            );
            earthOrientationSampleComputed = true;
        }

        if (!earthOrientationSample.isSuccess()) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            return std::nullopt;
        }

        mergeEarthOrientationWarnings(metadata, earthOrientationSample);
        return earthOrientationSample;
    }

    [[nodiscard]] std::optional<AstronomicalEpoch> ut1Epoch(EphemerisEngineQueryResult& metadata) const
    {
        if (!request.epoch.isFinite()) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            return std::nullopt;
        }

        if (request.epoch.timeScale == TimeScale::Ut1 || earthOrientationProvider == nullptr) {
            return epochInScale(TimeScale::Ut1, metadata);
        }

        const std::optional<EarthOrientationSample> sample = earthOrientation(metadata);
        if (!sample.has_value()) {
            return std::nullopt;
        }

        return addSeconds(sample->requestedUtcEpoch, sample->ut1MinusUtcSeconds, TimeScale::Ut1);
    }

private:
    [[nodiscard]] std::optional<TimeScaleConversionResult>* conversionCacheFor(const TimeScale targetScale) const
    {
        switch (targetScale) {
        case TimeScale::Tt:
            return &ttConversion;
        case TimeScale::Utc:
            return &utcConversion;
        case TimeScale::Ut1:
            return &ut1Conversion;
        case TimeScale::Tai:
        case TimeScale::Tdb:
            return nullptr;
        }

        return nullptr;
    }
};

[[nodiscard]] std::optional<Matrix3x3>
celestialIntermediateMatrix(const FrameTransformContext& context, EphemerisEngineQueryResult& metadata)
{
    if (!context.celestialIntermediateMatrixComputed) {
        EphemerisEngineQueryResult cachedMetadata;
        const std::optional<AstronomicalEpoch> ttEpoch = context.epochInScale(TimeScale::Tt, cachedMetadata);
        if (ttEpoch.has_value()) {
            context.celestialIntermediateMatrixValue = ErfaAstrometry::celestialToIntermediateMatrix06A(*ttEpoch);
            if (!context.celestialIntermediateMatrixValue.has_value()) {
                cachedMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
                cachedMetadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            }
        }
        context.celestialIntermediateMatrixMetadata = cachedMetadata;
        context.celestialIntermediateMatrixComputed = true;
    }

    mergeCachedTransformMetadata(metadata, context.celestialIntermediateMatrixMetadata);
    return context.celestialIntermediateMatrixValue;
}

[[nodiscard]] std::optional<Matrix3x3>
intermediateToTerrestrialIntermediateMatrix(const FrameTransformContext& context, EphemerisEngineQueryResult& metadata)
{
    if (!context.earthRotationMatrixComputed) {
        EphemerisEngineQueryResult cachedMetadata;
        const std::optional<AstronomicalEpoch> ut1Epoch = context.ut1Epoch(cachedMetadata);
        if (ut1Epoch.has_value()) {
            const std::optional<double> earthRotationAngle = ErfaAstrometry::earthRotationAngle00(*ut1Epoch);
            if (earthRotationAngle.has_value()) {
                context.earthRotationMatrixValue = earthRotationMatrix(*earthRotationAngle);
            } else {
                cachedMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
                cachedMetadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            }
        }
        context.earthRotationMatrixMetadata = cachedMetadata;
        context.earthRotationMatrixComputed = true;
    }

    mergeCachedTransformMetadata(metadata, context.earthRotationMatrixMetadata);
    return context.earthRotationMatrixValue;
}

[[nodiscard]] std::optional<Matrix3x3>
terrestrialIntermediateToTerrestrialMatrix(const FrameTransformContext& context, EphemerisEngineQueryResult& metadata)
{
    if (!context.polarMotionMatrixComputed) {
        EphemerisEngineQueryResult cachedMetadata;
        const std::optional<AstronomicalEpoch> ttEpoch = context.epochInScale(TimeScale::Tt, cachedMetadata);
        const std::optional<EarthOrientationSample> earthOrientationSample = context.earthOrientation(cachedMetadata);
        if (ttEpoch.has_value() && earthOrientationSample.has_value()) {
            const std::optional<double> tioLocator = ErfaAstrometry::tioLocatorS00(*ttEpoch);
            if (tioLocator.has_value()) {
                context.polarMotionMatrixValue = ErfaAstrometry::polarMotionMatrix00(
                    earthOrientationSample->polarMotionXArcseconds * MathConstants::kArcsecondsToRadians,
                    earthOrientationSample->polarMotionYArcseconds * MathConstants::kArcsecondsToRadians,
                    *tioLocator
                );
                if (!context.polarMotionMatrixValue.has_value()) {
                    cachedMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
                    cachedMetadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
                }
            } else {
                cachedMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
                cachedMetadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            }
        }
        context.polarMotionMatrixMetadata = cachedMetadata;
        context.polarMotionMatrixComputed = true;
    }

    mergeCachedTransformMetadata(metadata, context.polarMotionMatrixMetadata);
    return context.polarMotionMatrixValue;
}

[[nodiscard]] std::optional<Matrix3x3>
stageMatrix(const FrameTransformContext& context, const std::uint8_t lowerRank, EphemerisEngineQueryResult& metadata)
{
    switch (lowerRank) {
    case 0U:
        return celestialIntermediateMatrix(context, metadata);
    case 1U:
        return intermediateToTerrestrialIntermediateMatrix(context, metadata);
    case 2U:
        return terrestrialIntermediateToTerrestrialMatrix(context, metadata);
    default:
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        return std::nullopt;
    }
}

void mergeStageMetadata(
    EphemerisEngineQueryResult& aggregateMetadata, const EphemerisEngineQueryResult& stageMetadata
) noexcept
{
    EphemerisMetadataMerger::merge(
        aggregateMetadata,
        stageMetadata,
        EphemerisMetadataMergeOptions{
            .statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly,
            .mergeCorrections = true,
            .mergeProvenance = false,
            .mergeValidityRange = false,
            .mergeAngularUncertainty = false,
        }
    );
}

[[nodiscard]] CelestialFrameTransformStageMetadata
makeStageMetadata(const CelestialReferenceFrame sourceFrame, const CelestialReferenceFrame targetFrame)
{
    CelestialFrameTransformStageMetadata stage;
    stage.sourceFrame = sourceFrame;
    stage.targetFrame = targetFrame;
    stage.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    stage.metadata.dataSourceProvenance = kFrameTransformProvenance;
    return stage;
}

[[nodiscard]] constexpr EphemerisCorrectionFlags correctionForStage(const std::uint8_t lowerRank) noexcept
{
    return lowerRank == 0U ? EphemerisCorrectionFlags::precessionNutation()
                           : EphemerisCorrectionFlags::earthOrientation();
}

[[nodiscard]] bool isGcrsLike(const CelestialReferenceFrame frame) noexcept
{
    return frame == CelestialReferenceFrame::Icrs || frame == CelestialReferenceFrame::Gcrs;
}

[[nodiscard]] bool isTrueEquatorAndEquinox(const CelestialReferenceFrame frame) noexcept
{
    return frame == CelestialReferenceFrame::TrueEquatorAndEquinox;
}

[[nodiscard]] std::optional<Matrix3x3>
apparentEquatorAndEquinoxMatrix(const FrameTransformContext& context, EphemerisEngineQueryResult& metadata)
{
    if (!context.apparentEquatorAndEquinoxMatrixComputed) {
        EphemerisEngineQueryResult cachedMetadata;
        const std::optional<AstronomicalEpoch> ttEpoch = context.epochInScale(TimeScale::Tt, cachedMetadata);
        if (ttEpoch.has_value()) {
            context.apparentEquatorAndEquinoxMatrixValue = ErfaAstrometry::precessionNutationMatrix06A(*ttEpoch);
            if (!context.apparentEquatorAndEquinoxMatrixValue.has_value()) {
                cachedMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
                cachedMetadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            }
        }
        context.apparentEquatorAndEquinoxMatrixMetadata = cachedMetadata;
        context.apparentEquatorAndEquinoxMatrixComputed = true;
    }

    mergeCachedTransformMetadata(metadata, context.apparentEquatorAndEquinoxMatrixMetadata);
    return context.apparentEquatorAndEquinoxMatrixValue;
}

[[nodiscard]] std::optional<CelestialFrameTransformResult> tryTransformApparentEquatorAndEquinox(
    const CelestialFrameTransformRequest& request, const FrameTransformContext& context
)
{
    const bool forward =
        isGcrsLike(request.sourceFrame) && request.targetFrame == CelestialReferenceFrame::TrueEquatorAndEquinox;
    const bool reverse =
        request.sourceFrame == CelestialReferenceFrame::TrueEquatorAndEquinox && isGcrsLike(request.targetFrame);
    if (!forward && !reverse) {
        return std::nullopt;
    }

    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;

    CelestialFrameTransformStageMetadata stage = makeStageMetadata(request.sourceFrame, request.targetFrame);
    const std::optional<Matrix3x3> matrix = apparentEquatorAndEquinoxMatrix(context, stage.metadata);
    if (!matrix.has_value()) {
        mergeStageMetadata(result.metadata, stage.metadata);
        result.stages.push_back(std::move(stage));
        return result;
    }

    result.vector = forward ? multiply(*matrix, request.vector) : multiplyTranspose(*matrix, request.vector);
    stage.applied = true;
    stage.metadata.appliedCorrections = EphemerisCorrectionFlags::precessionNutation();
    mergeStageMetadata(result.metadata, stage.metadata);
    result.stages.push_back(std::move(stage));
    return result;
}

[[nodiscard]] CelestialFrameTransformResult
transformCelestialVectorWithContext(const CelestialFrameTransformRequest& request, const FrameTransformContext& context)
{
    if (!isFiniteVector(request.vector)) {
        return makeFailedResult(EphemerisEngineWarning::Code::ComputationFailed);
    }

    const std::uint8_t sourceRank = frameRank(request.sourceFrame);
    const std::uint8_t targetRank = frameRank(request.targetFrame);
    if (sourceRank == targetRank
        && (request.sourceFrame == request.targetFrame
            || (isGcrsLike(request.sourceFrame) && isGcrsLike(request.targetFrame)))) {
        return makeIdentityResult(request);
    }

    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;

    if (std::optional<CelestialFrameTransformResult> apparentResult =
            tryTransformApparentEquatorAndEquinox(request, context);
        apparentResult.has_value()) {
        return *apparentResult;
    }
    if (isTrueEquatorAndEquinox(request.sourceFrame) || isTrueEquatorAndEquinox(request.targetFrame)) {
        return makeFailedResult(EphemerisEngineWarning::Code::CorrectionUnavailable);
    }
    if (sourceRank == targetRank) {
        return makeFailedResult(EphemerisEngineWarning::Code::CorrectionUnavailable);
    }

    skygate::core::Vector3d transformed = request.vector;
    if (sourceRank < targetRank) {
        for (std::uint8_t lowerRank = sourceRank; lowerRank < targetRank; ++lowerRank) {
            const CelestialReferenceFrame stageSourceFrame =
                lowerRank == sourceRank ? request.sourceFrame : frameForRank(lowerRank);
            const std::uint8_t upperRank = static_cast<std::uint8_t>(lowerRank + 1U);
            const CelestialReferenceFrame stageTargetFrame =
                upperRank == targetRank ? request.targetFrame : frameForRank(upperRank);
            CelestialFrameTransformStageMetadata stage = makeStageMetadata(stageSourceFrame, stageTargetFrame);
            const std::optional<Matrix3x3> matrix = stageMatrix(context, lowerRank, stage.metadata);
            if (!matrix.has_value()) {
                mergeStageMetadata(result.metadata, stage.metadata);
                result.stages.push_back(std::move(stage));
                return result;
            }

            transformed = multiply(*matrix, transformed);
            stage.applied = true;
            stage.metadata.appliedCorrections = correctionForStage(lowerRank);
            mergeStageMetadata(result.metadata, stage.metadata);
            result.stages.push_back(std::move(stage));
        }
    } else {
        for (std::uint8_t lowerRank = sourceRank; lowerRank > targetRank; --lowerRank) {
            const std::uint8_t stageLowerRank = static_cast<std::uint8_t>(lowerRank - 1U);
            const CelestialReferenceFrame stageSourceFrame =
                lowerRank == sourceRank ? request.sourceFrame : frameForRank(lowerRank);
            const CelestialReferenceFrame stageTargetFrame =
                stageLowerRank == targetRank ? request.targetFrame : frameForRank(stageLowerRank);
            CelestialFrameTransformStageMetadata stage = makeStageMetadata(stageSourceFrame, stageTargetFrame);
            const std::optional<Matrix3x3> matrix = stageMatrix(context, stageLowerRank, stage.metadata);
            if (!matrix.has_value()) {
                mergeStageMetadata(result.metadata, stage.metadata);
                result.stages.push_back(std::move(stage));
                return result;
            }

            transformed = multiplyTranspose(*matrix, transformed);
            stage.applied = true;
            stage.metadata.appliedCorrections = correctionForStage(stageLowerRank);
            mergeStageMetadata(result.metadata, stage.metadata);
            result.stages.push_back(std::move(stage));
        }
    }

    result.vector = transformed;
    return result;
}

}  // namespace

ErfaFrameTransformer::ErfaFrameTransformer(
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider
)
    : m_timeScaleService(std::move(timeScaleService)), m_earthOrientationProvider(std::move(earthOrientationProvider))
{
}

CelestialFrameTransformResult
ErfaFrameTransformer::transformCelestialVector(const CelestialFrameTransformRequest& request) const
{
    const FrameTransformContext context{
        .request = request,
        .timeScaleService = m_timeScaleService.get(),
        .earthOrientationProvider = m_earthOrientationProvider.get(),
    };
    return transformCelestialVectorWithContext(request, context);
}

std::vector<CelestialFrameTransformResult>
ErfaFrameTransformer::transformCelestialVectors(const CelestialFrameBatchTransformRequest& request) const
{
    std::vector<CelestialFrameTransformResult> results;
    results.reserve(request.vectors.size());
    if (request.vectors.empty()) {
        return results;
    }

    const CelestialFrameTransformRequest contextRequest{
        .sourceFrame = request.sourceFrame,
        .targetFrame = request.targetFrame,
        .epoch = request.epoch,
        .vector = request.vectors.front(),
    };
    const FrameTransformContext context{
        .request = contextRequest,
        .timeScaleService = m_timeScaleService.get(),
        .earthOrientationProvider = m_earthOrientationProvider.get(),
    };
    for (const skygate::core::Vector3d& vector : request.vectors) {
        results.push_back(transformCelestialVectorWithContext(
            CelestialFrameTransformRequest{
                .sourceFrame = request.sourceFrame,
                .targetFrame = request.targetFrame,
                .epoch = request.epoch,
                .vector = vector,
            },
            context
        ));
    }

    return results;
}

}  // namespace skygate::ephemeris::highprecision
