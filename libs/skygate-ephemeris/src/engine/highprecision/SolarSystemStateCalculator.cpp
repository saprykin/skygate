#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include "StringUtilities.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;
constexpr double kHoursPerRadian = 12.0 / 3.141592653589793238462643383279502884;
constexpr double kDegreesPerRadian = 180.0 / 3.141592653589793238462643383279502884;
constexpr double kSpeedOfLightAuPerDay = 173.144632674240;
constexpr int kLightTimeIterationCount = 3;

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] std::optional<int> naifIdForBody(const CelestialBody& body) noexcept
{
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Sun || body.type == CelestialBodyType::Sun) {
        return 10;
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

[[nodiscard]] SolarSystemKernelVector
relativeVector(const SolarSystemKernelVector& target, const SolarSystemKernelVector& center) noexcept
{
    return {
        .xAu = target.xAu - center.xAu,
        .yAu = target.yAu - center.yAu,
        .zAu = target.zAu - center.zAu,
    };
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

void markLightTimeUnavailable(EphemerisResultMetadata& metadata) noexcept
{
    if (metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
    }
    metadata.addWarning(EphemerisWarningCode::CorrectionUnavailable);
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
    if (hasCorrectionFlag(input.request.options.correctionFlags, EphemerisCorrectionFlags::LightTime)) {
        const SolarSystemKernelStateResult earthState =
            m_kernelProvider->computeGeometricState(input.request.epoch, kNaifEarth, kNaifSolarSystemBarycenter);
        if (!earthState.positionAu.has_value()) {
            result.metadata.warningCodeMask |= earthState.metadata.warningCodeMask;
            markLightTimeUnavailable(result.metadata);
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
                markLightTimeUnavailable(result.metadata);
            }
        }
    }

    result.equatorial = equatorialFromVector(outputVector);
    if (!result.equatorial.has_value()) {
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
