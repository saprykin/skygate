#include "engine/highprecision/FrameTransformer.hpp"

#include "engine/highprecision/ErfaAstrometry.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr std::string_view kFrameTransformProvenance = "ERFA IAU 2006/2000A celestial and terrestrial frame transform";
constexpr double kArcsecondsToRadians = 4.8481368110953599359e-6;

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] bool isFiniteVector(const CelestialFrameVector& vector) noexcept
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

[[nodiscard]] std::uint8_t frameRank(const CelestialReferenceFrame frame) noexcept
{
    switch (frame) {
    case CelestialReferenceFrame::Icrs:
    case CelestialReferenceFrame::Gcrs:
        return 0U;
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

[[nodiscard]] CelestialFrameTransformResult makeFailedResult(const EphemerisWarningCode warningCode)
{
    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisResultStatus::Failed;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;
    return result;
}

[[nodiscard]] CelestialFrameTransformResult makeIdentityResult(const CelestialFrameTransformRequest& request)
{
    CelestialFrameTransformResult result;
    result.vector = request.vector;
    result.metadata.status = EphemerisResultStatus::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::NoCorrections;
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;
    return result;
}

[[nodiscard]] CelestialFrameVector multiply(const Matrix3x3& matrix, const CelestialFrameVector& vector) noexcept
{
    return {
        .x = matrix[0][0] * vector.x + matrix[0][1] * vector.y + matrix[0][2] * vector.z,
        .y = matrix[1][0] * vector.x + matrix[1][1] * vector.y + matrix[1][2] * vector.z,
        .z = matrix[2][0] * vector.x + matrix[2][1] * vector.y + matrix[2][2] * vector.z,
    };
}

[[nodiscard]] CelestialFrameVector
multiplyTranspose(const Matrix3x3& matrix, const CelestialFrameVector& vector) noexcept
{
    return {
        .x = matrix[0][0] * vector.x + matrix[1][0] * vector.y + matrix[2][0] * vector.z,
        .y = matrix[0][1] * vector.x + matrix[1][1] * vector.y + matrix[2][1] * vector.z,
        .z = matrix[0][2] * vector.x + matrix[1][2] * vector.y + matrix[2][2] * vector.z,
    };
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

void addDegradedWarning(EphemerisResultMetadata& metadata, const EphemerisWarningCode warningCode) noexcept
{
    if (metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
    }
    metadata.addWarning(warningCode);
}

void mergeTimeScaleWarnings(EphemerisResultMetadata& metadata, const TimeScaleConversionResult& conversion) noexcept
{
    if (conversion.status != TimeScaleConversionStatus::Degraded) {
        return;
    }

    addDegradedWarning(metadata, EphemerisWarningCode::AccuracyDegraded);
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

void mergeEarthOrientationWarnings(EphemerisResultMetadata& metadata, const EarthOrientationSample& sample) noexcept
{
    if (sample.status != EarthOrientationSampleStatus::Degraded) {
        return;
    }

    addDegradedWarning(metadata, EphemerisWarningCode::AccuracyDegraded);
    if (sample.hasWarning(EarthOrientationSampleWarningCode::MissingData)) {
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::EpochOutsideRange)) {
        metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    }
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

    [[nodiscard]] std::optional<AstronomicalEpoch>
    epochInScale(TimeScale targetScale, EphemerisResultMetadata& metadata) const
    {
        if (!isFiniteEpoch(request.epoch)) {
            metadata.status = EphemerisResultStatus::Failed;
            metadata.addWarning(EphemerisWarningCode::ComputationFailed);
            return std::nullopt;
        }

        if (request.epoch.timeScale == targetScale) {
            return normalizedAstronomicalEpoch(request.epoch);
        }

        if (timeScaleService == nullptr) {
            metadata.status = EphemerisResultStatus::Failed;
            metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            return std::nullopt;
        }

        std::optional<TimeScaleConversionResult>* cachedConversion = conversionCacheFor(targetScale);
        if (cachedConversion == nullptr) {
            metadata.status = EphemerisResultStatus::Failed;
            metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            return std::nullopt;
        }
        if (!cachedConversion->has_value()) {
            *cachedConversion = timeScaleService->convert(request.epoch, targetScale);
        }

        const TimeScaleConversionResult& conversion = **cachedConversion;
        if (!conversion.isSuccess()) {
            metadata.status = EphemerisResultStatus::Failed;
            metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            return std::nullopt;
        }

        mergeTimeScaleWarnings(metadata, conversion);
        return conversion.epoch;
    }

    [[nodiscard]] std::optional<EarthOrientationSample> earthOrientation(EphemerisResultMetadata& metadata) const
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
                }
            );
            earthOrientationSampleComputed = true;
        }

        if (!earthOrientationSample.isSuccess()) {
            metadata.status = EphemerisResultStatus::Failed;
            metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            return std::nullopt;
        }

        mergeEarthOrientationWarnings(metadata, earthOrientationSample);
        return earthOrientationSample;
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
celestialIntermediateMatrix(const FrameTransformContext& context, EphemerisResultMetadata& metadata)
{
    const std::optional<AstronomicalEpoch> ttEpoch = context.epochInScale(TimeScale::Tt, metadata);
    if (!ttEpoch.has_value()) {
        return std::nullopt;
    }

    const std::optional<Matrix3x3> celestialToIntermediate = celestialToIntermediateMatrix06A(JulianDateParts{
        .day1 = ttEpoch->julianDatePart1,
        .day2 = ttEpoch->julianDatePart2,
    });
    if (!celestialToIntermediate.has_value()) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return std::nullopt;
    }

    return celestialToIntermediate;
}

[[nodiscard]] std::optional<Matrix3x3>
intermediateToTerrestrialIntermediateMatrix(const FrameTransformContext& context, EphemerisResultMetadata& metadata)
{
    const std::optional<AstronomicalEpoch> ut1Epoch = context.epochInScale(TimeScale::Ut1, metadata);
    if (!ut1Epoch.has_value()) {
        return std::nullopt;
    }

    const std::optional<double> earthRotationAngle = earthRotationAngle00(JulianDateParts{
        .day1 = ut1Epoch->julianDatePart1,
        .day2 = ut1Epoch->julianDatePart2,
    });
    if (!earthRotationAngle.has_value()) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return std::nullopt;
    }

    return earthRotationMatrix(*earthRotationAngle);
}

[[nodiscard]] std::optional<Matrix3x3>
terrestrialIntermediateToTerrestrialMatrix(const FrameTransformContext& context, EphemerisResultMetadata& metadata)
{
    const std::optional<AstronomicalEpoch> ttEpoch = context.epochInScale(TimeScale::Tt, metadata);
    const std::optional<EarthOrientationSample> earthOrientationSample = context.earthOrientation(metadata);
    if (!ttEpoch.has_value() || !earthOrientationSample.has_value()) {
        return std::nullopt;
    }

    const std::optional<double> tioLocator = tioLocatorS00(JulianDateParts{
        .day1 = ttEpoch->julianDatePart1,
        .day2 = ttEpoch->julianDatePart2,
    });
    if (!tioLocator.has_value()) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return std::nullopt;
    }

    const std::optional<Matrix3x3> polarMotionMatrix = polarMotionMatrix00(
        earthOrientationSample->polarMotionXArcseconds * kArcsecondsToRadians,
        earthOrientationSample->polarMotionYArcseconds * kArcsecondsToRadians,
        *tioLocator
    );
    if (!polarMotionMatrix.has_value()) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return std::nullopt;
    }

    return polarMotionMatrix;
}

[[nodiscard]] std::optional<Matrix3x3>
stageMatrix(const FrameTransformContext& context, const std::uint8_t lowerRank, EphemerisResultMetadata& metadata)
{
    switch (lowerRank) {
    case 0U:
        return celestialIntermediateMatrix(context, metadata);
    case 1U:
        return intermediateToTerrestrialIntermediateMatrix(context, metadata);
    case 2U:
        return terrestrialIntermediateToTerrestrialMatrix(context, metadata);
    default:
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return std::nullopt;
    }
}

void mergeStageMetadata(
    EphemerisResultMetadata& aggregateMetadata, const EphemerisResultMetadata& stageMetadata
) noexcept
{
    if (stageMetadata.status == EphemerisResultStatus::Failed) {
        aggregateMetadata.status = EphemerisResultStatus::Failed;
    } else if (stageMetadata.status == EphemerisResultStatus::Degraded
               && aggregateMetadata.status == EphemerisResultStatus::Valid) {
        aggregateMetadata.status = EphemerisResultStatus::Degraded;
    }

    aggregateMetadata.warningCodeMask |= stageMetadata.warningCodeMask;
    aggregateMetadata.appliedCorrections |= stageMetadata.appliedCorrections;
}

[[nodiscard]] CelestialFrameTransformStageMetadata
makeStageMetadata(const CelestialReferenceFrame sourceFrame, const CelestialReferenceFrame targetFrame)
{
    CelestialFrameTransformStageMetadata stage;
    stage.sourceFrame = sourceFrame;
    stage.targetFrame = targetFrame;
    stage.metadata.status = EphemerisResultStatus::Valid;
    stage.metadata.dataSourceProvenance = kFrameTransformProvenance;
    return stage;
}

[[nodiscard]] constexpr EphemerisCorrectionFlags correctionForStage(const std::uint8_t lowerRank) noexcept
{
    return lowerRank == 0U ? EphemerisCorrectionFlags::PrecessionNutation : EphemerisCorrectionFlags::EarthOrientation;
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
    if (!isFiniteVector(request.vector)) {
        return makeFailedResult(EphemerisWarningCode::ComputationFailed);
    }

    const std::uint8_t sourceRank = frameRank(request.sourceFrame);
    const std::uint8_t targetRank = frameRank(request.targetFrame);
    if (sourceRank == targetRank) {
        return makeIdentityResult(request);
    }

    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisResultStatus::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::NoCorrections;
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;

    const FrameTransformContext context{
        .request = request,
        .timeScaleService = m_timeScaleService.get(),
        .earthOrientationProvider = m_earthOrientationProvider.get(),
    };
    CelestialFrameVector transformed = request.vector;
    if (sourceRank < targetRank) {
        for (std::uint8_t lowerRank = sourceRank; lowerRank < targetRank; ++lowerRank) {
            CelestialFrameTransformStageMetadata stage =
                makeStageMetadata(frameForRank(lowerRank), frameForRank(static_cast<std::uint8_t>(lowerRank + 1U)));
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
            CelestialFrameTransformStageMetadata stage =
                makeStageMetadata(frameForRank(lowerRank), frameForRank(stageLowerRank));
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

}  // namespace skygate::ephemeris::highprecision
