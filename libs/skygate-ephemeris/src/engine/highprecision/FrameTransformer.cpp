#include "engine/highprecision/FrameTransformer.hpp"

#include "engine/highprecision/ErfaAstrometry.hpp"

#include <cmath>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr std::string_view kFrameTransformProvenance = "ERFA IAU 2006/2000A celestial frame transform";

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] bool isFiniteVector(const CelestialFrameVector& vector) noexcept
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

[[nodiscard]] bool isCirsStage(const CelestialReferenceFrame frame) noexcept
{
    return frame == CelestialReferenceFrame::Cirs;
}

[[nodiscard]] bool requiresCelestialIntermediateMatrix(const CelestialFrameTransformRequest& request) noexcept
{
    return request.sourceFrame != request.targetFrame
           && (isCirsStage(request.sourceFrame) || isCirsStage(request.targetFrame));
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

[[nodiscard]] std::optional<AstronomicalEpoch> terrestrialTimeEpoch(
    const AstronomicalEpoch& epoch,
    const skygate::ephemeris::ITimeScaleService* timeScaleService,
    EphemerisResultMetadata& metadata
)
{
    if (!isFiniteEpoch(epoch)) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return std::nullopt;
    }

    if (epoch.timeScale == TimeScale::Tt) {
        return normalizedAstronomicalEpoch(epoch);
    }

    if (timeScaleService == nullptr) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return std::nullopt;
    }

    const TimeScaleConversionResult conversion = timeScaleService->convert(epoch, TimeScale::Tt);
    if (!conversion.isSuccess()) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return std::nullopt;
    }

    if (conversion.status == TimeScaleConversionStatus::Degraded) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    }

    return conversion.epoch;
}

}  // namespace

ErfaFrameTransformer::ErfaFrameTransformer(std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService
)
    : m_timeScaleService(std::move(timeScaleService))
{
}

CelestialFrameTransformResult
ErfaFrameTransformer::transformCelestialVector(const CelestialFrameTransformRequest& request) const
{
    if (!isFiniteVector(request.vector)) {
        return makeFailedResult(EphemerisWarningCode::ComputationFailed);
    }

    if (!requiresCelestialIntermediateMatrix(request)) {
        return makeIdentityResult(request);
    }

    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisResultStatus::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
    result.metadata.dataSourceProvenance = kFrameTransformProvenance;

    const std::optional<AstronomicalEpoch> ttEpoch =
        terrestrialTimeEpoch(request.epoch, m_timeScaleService.get(), result.metadata);
    if (!ttEpoch.has_value()) {
        return result;
    }

    const std::optional<Matrix3x3> celestialToIntermediate = celestialToIntermediateMatrix06A(JulianDateParts{
        .day1 = ttEpoch->julianDatePart1,
        .day2 = ttEpoch->julianDatePart2,
    });
    if (!celestialToIntermediate.has_value()) {
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return result;
    }

    if (!isCirsStage(request.sourceFrame) && isCirsStage(request.targetFrame)) {
        result.vector = multiply(*celestialToIntermediate, request.vector);
        return result;
    }

    if (isCirsStage(request.sourceFrame) && !isCirsStage(request.targetFrame)) {
        result.vector = multiplyTranspose(*celestialToIntermediate, request.vector);
        return result;
    }

    result.vector = request.vector;
    return result;
}

}  // namespace skygate::ephemeris::highprecision
