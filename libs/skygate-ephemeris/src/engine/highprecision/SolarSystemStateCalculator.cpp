#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include "StringUtilities.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;
constexpr int kNaifSun = 10;
constexpr double kHoursPerRadian = 12.0 / 3.141592653589793238462643383279502884;
constexpr double kDegreesPerRadian = 180.0 / 3.141592653589793238462643383279502884;
constexpr double kSpeedOfLightAuPerDay = 173.144632674240;
constexpr double kSolarSchwarzschildRadiusAu = 1.97412574336e-8;
constexpr int kLightTimeIterationCount = 3;

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] std::optional<int> naifIdForBody(const CelestialBody& body) noexcept
{
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Sun || body.type == CelestialBodyType::Sun) {
        return kNaifSun;
    }
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Moon || body.type == CelestialBodyType::Moon) {
        return 301;
    }
    if (body.ephemerisSource != CelestialBodyEphemerisSource::Planet && body.type != CelestialBodyType::Planet) {
        return std::nullopt;
    }

    if (strings::equalsIgnoreAsciiCase(body.id, "mercury")) {
        return 199;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "venus")) {
        return 299;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "mars")) {
        return 499;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "jupiter")) {
        return 599;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "saturn")) {
        return 699;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "uranus")) {
        return 799;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "neptune")) {
        return 899;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "pluto")) {
        return 999;
    }

    return std::nullopt;
}

[[nodiscard]] HighPrecisionCalculatorResult
makeStatusResult(const EphemerisResultStatus status, const EphemerisWarningCode warningCode)
{
    HighPrecisionCalculatorResult result;
    result.metadata.status = status;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    return result;
}

[[nodiscard]] std::optional<core::EquatorialCoordinate> equatorialFromVector(const SolarSystemKernelVector& vector
) noexcept
{
    if (!std::isfinite(vector.xAu) || !std::isfinite(vector.yAu) || !std::isfinite(vector.zAu)) {
        return std::nullopt;
    }

    const double xyDistance = std::hypot(vector.xAu, vector.yAu);
    const double distance = std::hypot(xyDistance, vector.zAu);
    if (distance <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    double rightAscensionHours = std::atan2(vector.yAu, vector.xAu) * kHoursPerRadian;
    if (rightAscensionHours < 0.0) {
        rightAscensionHours += 24.0;
    }

    return core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = std::atan2(vector.zAu, xyDistance) * kDegreesPerRadian,
    };
}

[[nodiscard]] double vectorDistanceAu(const SolarSystemKernelVector& vector) noexcept
{
    return std::hypot(std::hypot(vector.xAu, vector.yAu), vector.zAu);
}

[[nodiscard]] SolarSystemKernelVector scaleVector(const SolarSystemKernelVector& vector, const double scale) noexcept
{
    return {
        .xAu = vector.xAu * scale,
        .yAu = vector.yAu * scale,
        .zAu = vector.zAu * scale,
    };
}

[[nodiscard]] SolarSystemKernelVector
addVectors(const SolarSystemKernelVector& lhs, const SolarSystemKernelVector& rhs) noexcept
{
    return {
        .xAu = lhs.xAu + rhs.xAu,
        .yAu = lhs.yAu + rhs.yAu,
        .zAu = lhs.zAu + rhs.zAu,
    };
}

[[nodiscard]] SolarSystemKernelVector
relativeVector(const SolarSystemKernelVector& target, const SolarSystemKernelVector& center) noexcept
{
    return {
        .xAu = target.xAu - center.xAu,
        .yAu = target.yAu - center.yAu,
        .zAu = target.zAu - center.zAu,
    };
}

[[nodiscard]] double dotProduct(const SolarSystemKernelVector& lhs, const SolarSystemKernelVector& rhs) noexcept
{
    return lhs.xAu * rhs.xAu + lhs.yAu * rhs.yAu + lhs.zAu * rhs.zAu;
}

[[nodiscard]] std::optional<SolarSystemKernelVector> unitVector(const SolarSystemKernelVector& vector) noexcept
{
    const double distance = vectorDistanceAu(vector);
    if (!std::isfinite(distance) || distance <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    return scaleVector(vector, 1.0 / distance);
}

[[nodiscard]] std::optional<SolarSystemKernelVector> withDirectionPreservingDistance(
    const SolarSystemKernelVector& vector, const SolarSystemKernelVector& direction
) noexcept
{
    const double distance = vectorDistanceAu(vector);
    const std::optional<SolarSystemKernelVector> normalizedDirection = unitVector(direction);
    if (!std::isfinite(distance) || distance <= std::numeric_limits<double>::min()
        || !normalizedDirection.has_value()) {
        return std::nullopt;
    }

    return scaleVector(*normalizedDirection, distance);
}

[[nodiscard]] AstronomicalEpoch retardedEpoch(const AstronomicalEpoch& epoch, const double lightTimeDays) noexcept
{
    return normalizedAstronomicalEpoch(AstronomicalEpoch{
        .julianDatePart1 = epoch.julianDatePart1,
        .julianDatePart2 = epoch.julianDatePart2 - lightTimeDays,
        .timeScale = epoch.timeScale,
    });
}

void mergeKernelMetadata(EphemerisResultMetadata& target, const EphemerisResultMetadata& source) noexcept
{
    if (source.status == EphemerisResultStatus::Failed) {
        target.status = EphemerisResultStatus::Failed;
    } else if (source.status == EphemerisResultStatus::OutOfRange) {
        target.status = EphemerisResultStatus::OutOfRange;
    } else if (source.status == EphemerisResultStatus::Unsupported) {
        target.status = EphemerisResultStatus::Unsupported;
    } else if (source.status == EphemerisResultStatus::Degraded && target.status == EphemerisResultStatus::Valid) {
        target.status = EphemerisResultStatus::Degraded;
    }

    target.warningCodeMask |= source.warningCodeMask;
    if (target.dataSourceProvenance.empty()) {
        target.dataSourceProvenance = source.dataSourceProvenance;
    }
    if (!target.effectiveDataValidityRange.has_value()) {
        target.effectiveDataValidityRange = source.effectiveDataValidityRange;
    }
    if (!target.estimatedAngularUncertaintyArcsec.has_value()) {
        target.estimatedAngularUncertaintyArcsec = source.estimatedAngularUncertaintyArcsec;
    }
}

void markCorrectionUnavailable(
    EphemerisResultMetadata& metadata, const EphemerisCorrectionFlags unavailableCorrection
) noexcept
{
    if (metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
    }
    metadata.addUnavailableCorrection(unavailableCorrection);
}

[[nodiscard]] std::optional<SolarSystemKernelVector>
applyStellarAberration(const SolarSystemKernelVector& vector, const SolarSystemKernelVector& observerVelocityAuPerDay)
{
    const std::optional<SolarSystemKernelVector> direction = unitVector(vector);
    if (!direction.has_value()) {
        return std::nullopt;
    }

    const SolarSystemKernelVector beta = scaleVector(observerVelocityAuPerDay, 1.0 / kSpeedOfLightAuPerDay);
    const double directionDotBeta = dotProduct(*direction, beta);
    const SolarSystemKernelVector transverseBeta = relativeVector(beta, scaleVector(*direction, directionDotBeta));
    return withDirectionPreservingDistance(vector, addVectors(*direction, transverseBeta));
}

[[nodiscard]] std::optional<SolarSystemKernelVector>
applySolarGravitationalDeflection(const SolarSystemKernelVector& vector, const SolarSystemKernelVector& sunVector)
{
    const std::optional<SolarSystemKernelVector> targetDirection = unitVector(vector);
    const std::optional<SolarSystemKernelVector> sunDirection = unitVector(sunVector);
    const double observerSunDistanceAu = vectorDistanceAu(sunVector);
    if (!targetDirection.has_value() || !sunDirection.has_value() || !std::isfinite(observerSunDistanceAu)
        || observerSunDistanceAu <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    const double cosineElongation = std::clamp(dotProduct(*targetDirection, *sunDirection), -1.0, 1.0);
    const double denominator = std::max(1.0 - cosineElongation, 1.0e-12);
    const double deflectionScale = kSolarSchwarzschildRadiusAu / observerSunDistanceAu / denominator;
    const SolarSystemKernelVector awayFromSun =
        relativeVector(scaleVector(*targetDirection, cosineElongation), *sunDirection);
    return withDirectionPreservingDistance(
        vector, addVectors(*targetDirection, scaleVector(awayFromSun, deflectionScale))
    );
}

}  // namespace

SolarSystemStateCalculator::SolarSystemStateCalculator(std::shared_ptr<const ICalcephKernelProvider> kernelProvider)
    : m_kernelProvider(std::move(kernelProvider))
{
}

HighPrecisionCalculatorResult SolarSystemStateCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    if (!isFiniteEpoch(input.request.epoch)) {
        return makeStatusResult(EphemerisResultStatus::Failed, EphemerisWarningCode::ComputationFailed);
    }
    if (input.request.epoch.timeScale != TimeScale::Tdb) {
        return makeStatusResult(EphemerisResultStatus::Failed, EphemerisWarningCode::TimeScaleDataUnavailable);
    }
    if (m_kernelProvider == nullptr) {
        return makeStatusResult(EphemerisResultStatus::Failed, EphemerisWarningCode::MissingEphemerisData);
    }

    const std::optional<int> targetNaifId = naifIdForBody(input.body);
    if (!targetNaifId.has_value() || *targetNaifId == kNaifEarth) {
        return makeStatusResult(EphemerisResultStatus::Unsupported, EphemerisWarningCode::UnsupportedBody);
    }

    const SolarSystemKernelStateResult kernelResult =
        m_kernelProvider->computeGeometricState(input.request.epoch, *targetNaifId, kNaifEarth);
    HighPrecisionCalculatorResult result;
    result.metadata = kernelResult.metadata;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::Geometric;
    if (result.metadata.dataSourceProvenance.empty()) {
        result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    }

    if (!kernelResult.positionAu.has_value()) {
        if (result.metadata.status == EphemerisResultStatus::Valid) {
            result.metadata.status = EphemerisResultStatus::Failed;
            result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        }
        return result;
    }

    SolarSystemKernelVector outputVector = *kernelResult.positionAu;
    std::optional<SolarSystemKernelStateResult> earthBarycentricState;
    const auto observerState = [&]() -> const SolarSystemKernelStateResult& {
        if (!earthBarycentricState.has_value()) {
            earthBarycentricState =
                m_kernelProvider->computeGeometricState(input.request.epoch, kNaifEarth, kNaifSolarSystemBarycenter);
        }
        return *earthBarycentricState;
    };

    if (hasCorrectionFlag(input.request.options.correctionFlags, EphemerisCorrectionFlags::LightTime)) {
        const SolarSystemKernelStateResult& earthState = observerState();
        if (!earthState.positionAu.has_value()) {
            result.metadata.warningCodeMask |= earthState.metadata.warningCodeMask;
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::LightTime);
        } else {
            mergeKernelMetadata(result.metadata, earthState.metadata);
            double lightTimeDays = vectorDistanceAu(outputVector) / kSpeedOfLightAuPerDay;
            std::optional<SolarSystemKernelVector> correctedVector;
            for (int iteration = 0; iteration < kLightTimeIterationCount; ++iteration) {
                const AstronomicalEpoch targetEpoch = retardedEpoch(input.request.epoch, lightTimeDays);
                const SolarSystemKernelStateResult targetState =
                    m_kernelProvider->computeGeometricState(targetEpoch, *targetNaifId, kNaifSolarSystemBarycenter);
                if (!targetState.positionAu.has_value()) {
                    result.metadata.warningCodeMask |= targetState.metadata.warningCodeMask;
                    correctedVector = std::nullopt;
                    break;
                }

                mergeKernelMetadata(result.metadata, targetState.metadata);
                correctedVector = relativeVector(*targetState.positionAu, *earthState.positionAu);
                lightTimeDays = vectorDistanceAu(*correctedVector) / kSpeedOfLightAuPerDay;
            }

            if (correctedVector.has_value()
                && vectorDistanceAu(*correctedVector) > std::numeric_limits<double>::min()) {
                outputVector = *correctedVector;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::LightTime;
            } else {
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::LightTime);
            }
        }
    }

    if (hasCorrectionFlag(input.request.options.correctionFlags, EphemerisCorrectionFlags::GravitationalLightDeflection)
        && *targetNaifId != kNaifSun) {
        const SolarSystemKernelStateResult sunState =
            m_kernelProvider->computeGeometricState(input.request.epoch, kNaifSun, kNaifEarth);
        if (!sunState.positionAu.has_value()) {
            result.metadata.warningCodeMask |= sunState.metadata.warningCodeMask;
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::GravitationalLightDeflection);
        } else {
            mergeKernelMetadata(result.metadata, sunState.metadata);
            if (const std::optional<SolarSystemKernelVector> deflectedVector =
                    applySolarGravitationalDeflection(outputVector, *sunState.positionAu);
                deflectedVector.has_value()) {
                outputVector = *deflectedVector;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::GravitationalLightDeflection;
            } else {
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::GravitationalLightDeflection);
            }
        }
    }

    if (hasCorrectionFlag(input.request.options.correctionFlags, EphemerisCorrectionFlags::StellarAberration)) {
        const SolarSystemKernelStateResult& earthState = observerState();
        if (!earthState.positionAu.has_value() || !earthState.velocityAuPerDay.has_value()) {
            result.metadata.warningCodeMask |= earthState.metadata.warningCodeMask;
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::StellarAberration);
        } else {
            mergeKernelMetadata(result.metadata, earthState.metadata);
            if (const std::optional<SolarSystemKernelVector> aberratedVector =
                    applyStellarAberration(outputVector, *earthState.velocityAuPerDay);
                aberratedVector.has_value()) {
                outputVector = *aberratedVector;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::StellarAberration;
            } else {
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::StellarAberration);
            }
        }
    }

    result.observerRelativePositionAu = outputVector;
    result.equatorial = equatorialFromVector(outputVector);
    if (!result.equatorial.has_value()) {
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
