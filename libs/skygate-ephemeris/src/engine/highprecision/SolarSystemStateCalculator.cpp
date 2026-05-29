#include "SolarSystemStateCalculator.hpp"
#include "EphemerisMetadataMerge.hpp"
#include "ICalcephKernelProvider.hpp"
#include "StringUtilities.hpp"
#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/Vector3d.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;
constexpr int kNaifSun = 10;

using skygate::core::MathConstants;
using skygate::core::PhysicalConstants;
constexpr int kLightTimeIterationCount = 3;

struct TargetKernelState {
    SolarSystemKernelStateResult state;
    int targetNaifId = 0;
    int requestedTargetNaifId = 0;
};

[[nodiscard]] std::optional<int> naifIdForBody(const BaseCelestialBody& body) noexcept
{
    if (body.kind == BaseCelestialBody::Kind::Sun) {
        return kNaifSun;
    }
    if (body.kind == BaseCelestialBody::Kind::Moon) {
        return 301;
    }
    if (body.kind != BaseCelestialBody::Kind::Planet) {
        return std::nullopt;
    }

    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "mercury")) {
        return 199;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "venus")) {
        return 299;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "mars")) {
        return 499;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "jupiter")) {
        return 599;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "saturn")) {
        return 699;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "uranus")) {
        return 799;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "neptune")) {
        return 899;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "pluto")) {
        return 999;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<int> planetarySystemBarycenterNaifIdForBody(const BaseCelestialBody& body) noexcept
{
    if (body.kind != BaseCelestialBody::Kind::Planet) {
        return std::nullopt;
    }

    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "mercury")) {
        return 1;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "venus")) {
        return 2;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "mars")) {
        return 4;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "jupiter")) {
        return 5;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "saturn")) {
        return 6;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "uranus")) {
        return 7;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "neptune")) {
        return 8;
    }
    if (StringUtilities::equalsIgnoreAsciiCase(body.id, "pluto")) {
        return 9;
    }

    return std::nullopt;
}

[[nodiscard]] bool de440sHasPlanetBodyCenter(const BaseCelestialBody& body) noexcept
{
    return StringUtilities::equalsIgnoreAsciiCase(body.id, "mercury")
           || StringUtilities::equalsIgnoreAsciiCase(body.id, "venus");
}

[[nodiscard]] bool shouldPreferPlanetarySystemBarycenter(
    const BaseCelestialBody& body, const bool preferPlanetarySystemBarycenters
) noexcept
{
    if (!preferPlanetarySystemBarycenters) {
        return false;
    }
    if (body.kind != BaseCelestialBody::Kind::Planet) {
        return false;
    }

    return !de440sHasPlanetBodyCenter(body);
}

[[nodiscard]] std::string barycenterFallbackProvenance(
    const BaseCelestialBody& body, const int requestedTargetNaifId, const int effectiveTargetNaifId
)
{
    std::string bodyName = body.id.empty() ? body.displayName : body.id;
    if (bodyName.empty()) {
        bodyName = "planet";
    }

    return bodyName + " body center (" + std::to_string(requestedTargetNaifId)
           + ") served by planetary-system barycenter (" + std::to_string(effectiveTargetNaifId) + ")";
}

void appendProvenance(EphemerisEngineQueryResult& metadata, std::string provenance)
{
    if (provenance.empty()) {
        return;
    }
    if (metadata.dataSourceProvenance.empty()) {
        metadata.dataSourceProvenance = std::move(provenance);
        return;
    }

    metadata.dataSourceProvenance += "; ";
    metadata.dataSourceProvenance += provenance;
}

void markBarycenterFallback(
    EphemerisEngineQueryResult& metadata,
    const BaseCelestialBody& body,
    const int requestedTargetNaifId,
    const int effectiveTargetNaifId
)
{
    if (requestedTargetNaifId == effectiveTargetNaifId) {
        return;
    }
    if (metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
        metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
    const bool alreadyReported = metadata.hasWarning(EphemerisEngineWarning::Code::BarycenterFallback);
    metadata.addWarning(EphemerisEngineWarning::Code::BarycenterFallback);
    if (!alreadyReported) {
        appendProvenance(metadata, barycenterFallbackProvenance(body, requestedTargetNaifId, effectiveTargetNaifId));
    }
}

[[nodiscard]] HighPrecisionCalculatorResult
makeStatusResult(const EphemerisEngineQueryStatus::Type status, const EphemerisEngineWarning::Code warningCode)
{
    HighPrecisionCalculatorResult result;
    result.metadata.status = status;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    return result;
}

[[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
equatorialFromVector(const skygate::core::Vector3d& vector) noexcept
{
    if (!vector.isFinite()) {
        return std::nullopt;
    }

    const double xyDistance = std::hypot(vector.x, vector.y);
    const double distance = vector.length();
    if (distance <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    double rightAscensionHours = std::atan2(vector.y, vector.x) * MathConstants::kHoursPerRadian;
    if (rightAscensionHours < 0.0) {
        rightAscensionHours += 24.0;
    }

    return skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = std::atan2(vector.z, xyDistance) * MathConstants::kRadiansToDegrees,
    };
}

[[nodiscard]] std::optional<skygate::core::Vector3d> withDirectionPreservingDistance(
    const skygate::core::Vector3d& vector, const skygate::core::Vector3d& direction
) noexcept
{
    const double distance = vector.length();
    const std::optional<skygate::core::Vector3d> normalizedDirection = direction.normalized();
    if (!std::isfinite(distance) || distance <= std::numeric_limits<double>::min()
        || !normalizedDirection.has_value()) {
        return std::nullopt;
    }

    return *normalizedDirection * distance;
}

[[nodiscard]] AstronomicalEpoch retardedEpoch(const AstronomicalEpoch& epoch, const double lightTimeDays) noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = epoch.julianDatePart1,
        .julianDatePart2 = epoch.julianDatePart2 - lightTimeDays,
        .timeScale = epoch.timeScale,
    }
        .normalized();
}

[[nodiscard]] TargetKernelState computeTargetKernelState(
    const ICalcephKernelProvider& kernelProvider,
    const AstronomicalEpoch& epoch,
    const int targetNaifId,
    const std::optional<int> fallbackTargetNaifId,
    const int centerNaifId,
    const bool preferFallbackTarget
)
{
    if (preferFallbackTarget && fallbackTargetNaifId.has_value() && *fallbackTargetNaifId != targetNaifId) {
        const SolarSystemKernelStateResult preferredState =
            kernelProvider.computeGeometricState(epoch, *fallbackTargetNaifId, centerNaifId);
        if (preferredState.positionAu.has_value()
            || preferredState.metadata.status != EphemerisEngineQueryStatus::Type::Failed) {
            return TargetKernelState{
                .state = preferredState,
                .targetNaifId = *fallbackTargetNaifId,
                .requestedTargetNaifId = *fallbackTargetNaifId,
            };
        }
    }

    TargetKernelState result{
        .state = kernelProvider.computeGeometricState(epoch, targetNaifId, centerNaifId),
        .targetNaifId = targetNaifId,
        .requestedTargetNaifId = targetNaifId,
    };
    if (result.state.positionAu.has_value() || !fallbackTargetNaifId.has_value()
        || *fallbackTargetNaifId == targetNaifId) {
        return result;
    }
    if (result.state.metadata.status != EphemerisEngineQueryStatus::Type::Failed) {
        return result;
    }

    SolarSystemKernelStateResult fallbackState =
        kernelProvider.computeGeometricState(epoch, *fallbackTargetNaifId, centerNaifId);
    if (!fallbackState.positionAu.has_value()) {
        return result;
    }

    result.state = std::move(fallbackState);
    result.targetNaifId = *fallbackTargetNaifId;
    return result;
}

[[nodiscard]] std::optional<skygate::core::Vector3d>
applyStellarAberration(const skygate::core::Vector3d& vector, const skygate::core::Vector3d& observerVelocityAuPerDay)
{
    const std::optional<skygate::core::Vector3d> direction = vector.normalized();
    if (!direction.has_value()) {
        return std::nullopt;
    }

    const skygate::core::Vector3d beta = observerVelocityAuPerDay * (1.0 / PhysicalConstants::kSpeedOfLightAuPerDay);
    const double directionDotBeta = direction->dot(beta);
    const skygate::core::Vector3d transverseBeta = beta - (*direction * directionDotBeta);
    return withDirectionPreservingDistance(vector, *direction + transverseBeta);
}

[[nodiscard]] std::optional<skygate::core::Vector3d>
applySolarGravitationalDeflection(const skygate::core::Vector3d& vector, const skygate::core::Vector3d& sunVector)
{
    const std::optional<skygate::core::Vector3d> targetDirection = vector.normalized();
    const std::optional<skygate::core::Vector3d> sunDirection = sunVector.normalized();
    const double observerSunDistanceAu = sunVector.length();
    if (!targetDirection.has_value() || !sunDirection.has_value() || !std::isfinite(observerSunDistanceAu)
        || observerSunDistanceAu <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    const double cosineElongation = std::clamp(targetDirection->dot(*sunDirection), -1.0, 1.0);
    const double denominator = std::max(1.0 - cosineElongation, 1.0e-12);
    const double deflectionScale = PhysicalConstants::kSolarSchwarzschildRadiusAu / observerSunDistanceAu / denominator;
    const skygate::core::Vector3d awayFromSun = (*targetDirection * cosineElongation) - *sunDirection;
    return withDirectionPreservingDistance(vector, *targetDirection + (awayFromSun * deflectionScale));
}

}  // namespace

SolarSystemStateCalculator::SolarSystemStateCalculator(
    std::shared_ptr<const ICalcephKernelProvider> kernelProvider, const bool preferPlanetarySystemBarycenters
)
    : m_kernelProvider(std::move(kernelProvider)), m_preferPlanetarySystemBarycenters(preferPlanetarySystemBarycenters)
{
}

HighPrecisionCalculatorResult SolarSystemStateCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    if (!input.request.epoch.isFinite()) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Failed, EphemerisEngineWarning::Code::ComputationFailed
        );
    }
    if (input.request.epoch.timeScale != TimeScale::Tdb) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Failed, EphemerisEngineWarning::Code::TimeScaleDataUnavailable
        );
    }
    if (m_kernelProvider == nullptr) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Failed, EphemerisEngineWarning::Code::MissingEphemerisData
        );
    }

    const std::optional<int> targetNaifId = naifIdForBody(input.body);
    const std::optional<int> fallbackTargetNaifId = planetarySystemBarycenterNaifIdForBody(input.body);
    const bool preferPlanetarySystemBarycenter =
        shouldPreferPlanetarySystemBarycenter(input.body, m_preferPlanetarySystemBarycenters);
    if (!targetNaifId.has_value() || *targetNaifId == kNaifEarth) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Unsupported, EphemerisEngineWarning::Code::UnsupportedBody
        );
    }

    const TargetKernelState targetKernelResult = computeTargetKernelState(
        *m_kernelProvider,
        input.request.epoch,
        *targetNaifId,
        fallbackTargetNaifId,
        kNaifEarth,
        preferPlanetarySystemBarycenter
    );
    const SolarSystemKernelStateResult& kernelResult = targetKernelResult.state;
    const int effectiveTargetNaifId = targetKernelResult.targetNaifId;
    HighPrecisionCalculatorResult result;
    result.metadata = kernelResult.metadata;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::geometric();
    if (result.metadata.dataSourceProvenance.empty()) {
        result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    }
    markBarycenterFallback(
        result.metadata, input.body, targetKernelResult.requestedTargetNaifId, effectiveTargetNaifId
    );

    if (!kernelResult.positionAu.has_value()) {
        if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        }
        return result;
    }

    skygate::core::Vector3d outputVector = *kernelResult.positionAu;
    std::optional<SolarSystemKernelStateResult> earthBarycentricState;
    const auto observerState = [&]() -> const SolarSystemKernelStateResult& {
        if (!earthBarycentricState.has_value()) {
            earthBarycentricState =
                m_kernelProvider->computeGeometricState(input.request.epoch, kNaifEarth, kNaifSolarSystemBarycenter);
        }
        return *earthBarycentricState;
    };

    if (skygate::ephemeris::EphemerisCorrectionFlags::has(
            input.request.options.correctionFlags(), EphemerisCorrectionFlags::lightTime()
        )) {
        const SolarSystemKernelStateResult& earthState = observerState();
        if (!earthState.positionAu.has_value()) {
            result.metadata.warningCodeMask |= earthState.metadata.warningCodeMask;
            EphemerisMetadataMerger::markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::lightTime());
        } else {
            EphemerisMetadataMerger::merge(
                result.metadata, earthState.metadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
            );
            double lightTimeDays = outputVector.length() / PhysicalConstants::kSpeedOfLightAuPerDay;
            std::optional<skygate::core::Vector3d> correctedVector;
            for (int iteration = 0; iteration < kLightTimeIterationCount; ++iteration) {
                const AstronomicalEpoch targetEpoch = retardedEpoch(input.request.epoch, lightTimeDays);
                const TargetKernelState retardedTargetState = computeTargetKernelState(
                    *m_kernelProvider,
                    targetEpoch,
                    effectiveTargetNaifId,
                    fallbackTargetNaifId,
                    kNaifSolarSystemBarycenter,
                    preferPlanetarySystemBarycenter
                );
                const SolarSystemKernelStateResult& targetState = retardedTargetState.state;
                if (!targetState.positionAu.has_value()) {
                    result.metadata.warningCodeMask |= targetState.metadata.warningCodeMask;
                    correctedVector = std::nullopt;
                    break;
                }

                EphemerisMetadataMerger::merge(
                    result.metadata, targetState.metadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
                );
                markBarycenterFallback(
                    result.metadata,
                    input.body,
                    retardedTargetState.requestedTargetNaifId,
                    retardedTargetState.targetNaifId
                );
                correctedVector = *targetState.positionAu - *earthState.positionAu;
                lightTimeDays = correctedVector->length() / PhysicalConstants::kSpeedOfLightAuPerDay;
            }

            if (correctedVector.has_value() && correctedVector->length() > std::numeric_limits<double>::min()) {
                outputVector = *correctedVector;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::lightTime();
            } else {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::lightTime()
                );
            }
        }
    }

    if (skygate::ephemeris::EphemerisCorrectionFlags::has(
            input.request.options.correctionFlags(), EphemerisCorrectionFlags::gravitationalLightDeflection()
        )
        && effectiveTargetNaifId != kNaifSun) {
        const SolarSystemKernelStateResult sunState =
            m_kernelProvider->computeGeometricState(input.request.epoch, kNaifSun, kNaifEarth);
        if (!sunState.positionAu.has_value()) {
            result.metadata.warningCodeMask |= sunState.metadata.warningCodeMask;
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::gravitationalLightDeflection()
            );
        } else {
            EphemerisMetadataMerger::merge(
                result.metadata, sunState.metadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
            );
            if (const std::optional<skygate::core::Vector3d> deflectedVector =
                    applySolarGravitationalDeflection(outputVector, *sunState.positionAu);
                deflectedVector.has_value()) {
                outputVector = *deflectedVector;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::gravitationalLightDeflection();
            } else {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::gravitationalLightDeflection()
                );
            }
        }
    }

    if (skygate::ephemeris::EphemerisCorrectionFlags::has(
            input.request.options.correctionFlags(), EphemerisCorrectionFlags::stellarAberration()
        )) {
        const SolarSystemKernelStateResult& earthState = observerState();
        if (!earthState.positionAu.has_value() || !earthState.velocityAuPerDay.has_value()) {
            result.metadata.warningCodeMask |= earthState.metadata.warningCodeMask;
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::stellarAberration()
            );
        } else {
            EphemerisMetadataMerger::merge(
                result.metadata, earthState.metadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
            );
            if (const std::optional<skygate::core::Vector3d> aberratedVector =
                    applyStellarAberration(outputVector, *earthState.velocityAuPerDay);
                aberratedVector.has_value()) {
                outputVector = *aberratedVector;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::stellarAberration();
            } else {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::stellarAberration()
                );
            }
        }
    }

    result.observerRelativePositionAu = outputVector;
    result.equatorial = equatorialFromVector(outputVector);
    if (!result.equatorial.has_value()) {
        result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
