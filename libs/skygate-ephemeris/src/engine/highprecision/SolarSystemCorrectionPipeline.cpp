#include "SolarSystemCorrectionPipeline.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "NaifBodyIds.hpp"
#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::MathConstants;
using skygate::core::NaifBodyIds;
using skygate::core::PhysicalConstants;
using skygate::core::Vector3d;
using skygate::ephemeris::EphemerisCorrectionFlags;

constexpr int kLightTimeIterationCount = 3;

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

[[nodiscard]] skygate::core::AstronomicalEpoch
retardedEpoch(const skygate::core::AstronomicalEpoch& epoch, const double lightTimeDays) noexcept
{
    return epoch.addDays(-lightTimeDays);
}

[[nodiscard]] std::optional<Vector3d> applyAberration(const Vector3d& vector, const Vector3d& observerVelocityAuPerDay)
{
    const std::optional<Vector3d> direction = vector.normalized();
    if (!direction.has_value()) {
        return std::nullopt;
    }

    const Vector3d beta = observerVelocityAuPerDay * (1.0 / PhysicalConstants::kSpeedOfLightAuPerDay);
    const double directionDotBeta = direction->dot(beta);
    const Vector3d transverseBeta = beta - (*direction * directionDotBeta);
    return (*direction + transverseBeta).withLength(vector.length());
}

[[nodiscard]] std::optional<Vector3d> applySolarDeflection(const Vector3d& vector, const Vector3d& sunVector)
{
    const std::optional<Vector3d> targetDirection = vector.normalized();
    const std::optional<Vector3d> sunDirection = sunVector.normalized();
    const double observerSunDistanceAu = sunVector.length();
    if (!targetDirection.has_value() || !sunDirection.has_value() || !std::isfinite(observerSunDistanceAu)
        || observerSunDistanceAu <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    const double cosineElongation = std::clamp(targetDirection->dot(*sunDirection), -1.0, 1.0);
    const double denominator = std::max(1.0 - cosineElongation, MathConstants::kEpsilon);
    const double deflectionScale = PhysicalConstants::kSolarSchwarzschildRadiusAu / observerSunDistanceAu / denominator;
    const Vector3d awayFromSun = (*targetDirection * cosineElongation) - *sunDirection;
    return (*targetDirection + (awayFromSun * deflectionScale)).withLength(vector.length());
}

}  // namespace

SolarSystemCorrectionPipeline::SolarSystemCorrectionPipeline(
    const ICalcephKernel& kernel,
    const HighPrecisionComputationInput& input,
    EphemerisEngineQueryResult& metadata,
    const skygate::core::Vector3d outputVector,
    const int effectiveTargetNaifId,
    const std::optional<int> fallbackTargetNaifId,
    const bool preferPlanetarySystemBarycenter
)
    : m_kernel(kernel), m_input(input), m_metadata(metadata), m_outputVector(outputVector),
      m_effectiveTargetNaifId(effectiveTargetNaifId), m_fallbackTargetNaifId(fallbackTargetNaifId),
      m_preferPlanetarySystemBarycenter(preferPlanetarySystemBarycenter)
{
}

void SolarSystemCorrectionPipeline::applyCorrections(const EphemerisCorrectionFlags requestedFlags)
{
    if (EphemerisCorrectionFlags::has(requestedFlags, EphemerisCorrectionFlags::lightTime())) {
        applyLightTime();
    }
    if (EphemerisCorrectionFlags::has(requestedFlags, EphemerisCorrectionFlags::gravitationalLightDeflection())
        && m_effectiveTargetNaifId != NaifBodyIds::kSun) {
        applyGravitationalLightDeflection();
    }
    if (EphemerisCorrectionFlags::has(requestedFlags, EphemerisCorrectionFlags::stellarAberration())) {
        applyStellarAberration();
    }
}

skygate::core::Vector3d SolarSystemCorrectionPipeline::observerRelativePositionAu() const noexcept
{
    return m_outputVector;
}

SolarSystemCorrectionPipeline::TargetKernelState SolarSystemCorrectionPipeline::computeTargetKernelState(
    const ICalcephKernel& kernel,
    const skygate::core::AstronomicalEpoch& epoch,
    const int targetNaifId,
    const std::optional<int> fallbackTargetNaifId,
    const int centerNaifId,
    const bool preferFallbackTarget
)
{
    if (preferFallbackTarget && fallbackTargetNaifId.has_value() && *fallbackTargetNaifId != targetNaifId) {
        const ICalcephKernel::StateResult preferredState = kernel.compute(epoch, *fallbackTargetNaifId, centerNaifId);
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
        .state = kernel.compute(epoch, targetNaifId, centerNaifId),
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

    ICalcephKernel::StateResult fallbackState = kernel.compute(epoch, *fallbackTargetNaifId, centerNaifId);
    if (!fallbackState.positionAu.has_value()) {
        return result;
    }

    result.state = std::move(fallbackState);
    result.targetNaifId = *fallbackTargetNaifId;
    return result;
}

void SolarSystemCorrectionPipeline::markBarycenterFallback(
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

const ICalcephKernel::StateResult& SolarSystemCorrectionPipeline::observerState()
{
    if (!m_earthBarycentricState.has_value()) {
        m_earthBarycentricState =
            m_kernel.compute(m_input.request.epoch, NaifBodyIds::kEarth, NaifBodyIds::kSolarSystemBarycenter);
    }
    return *m_earthBarycentricState;
}

bool SolarSystemCorrectionPipeline::beginStage(
    const EphemerisCorrectionFlags correction,
    const ICalcephKernel::StateResult& dependencyState,
    const bool dependencyAvailable
)
{
    if (!dependencyAvailable) {
        EphemerisMetadataMerger::mergeWarnings(m_metadata, dependencyState.metadata);
        EphemerisMetadataMerger::markCorrectionUnavailable(m_metadata, correction);
        return false;
    }

    EphemerisMetadataMerger::merge(
        m_metadata, dependencyState.metadata, EphemerisMetadataMergeOptions::withoutCorrections()
    );
    return true;
}

void SolarSystemCorrectionPipeline::finishStage(
    const EphemerisCorrectionFlags correction, const std::optional<skygate::core::Vector3d> correctedVector
)
{
    if (!correctedVector.has_value()) {
        EphemerisMetadataMerger::markCorrectionUnavailable(m_metadata, correction);
        return;
    }

    m_outputVector = *correctedVector;
    EphemerisMetadataMerger::markCorrectionApplied(m_metadata, correction);
}

void SolarSystemCorrectionPipeline::applyLightTime()
{
    const ICalcephKernel::StateResult& earthState = observerState();
    if (!beginStage(EphemerisCorrectionFlags::lightTime(), earthState, earthState.positionAu.has_value())) {
        return;
    }

    double lightTimeDays = m_outputVector.length() / PhysicalConstants::kSpeedOfLightAuPerDay;
    std::optional<skygate::core::Vector3d> correctedVector;
    for (int iteration = 0; iteration < kLightTimeIterationCount; ++iteration) {
        const skygate::core::AstronomicalEpoch targetEpoch = retardedEpoch(m_input.request.epoch, lightTimeDays);
        const TargetKernelState retardedTargetState = computeTargetKernelState(
            m_kernel,
            targetEpoch,
            m_effectiveTargetNaifId,
            m_fallbackTargetNaifId,
            NaifBodyIds::kSolarSystemBarycenter,
            m_preferPlanetarySystemBarycenter
        );
        const ICalcephKernel::StateResult& targetState = retardedTargetState.state;
        if (!targetState.positionAu.has_value()) {
            EphemerisMetadataMerger::mergeWarnings(m_metadata, targetState.metadata);
            correctedVector = std::nullopt;
            break;
        }

        EphemerisMetadataMerger::merge(
            m_metadata, targetState.metadata, EphemerisMetadataMergeOptions::withoutCorrections()
        );
        markBarycenterFallback(
            m_metadata, m_input.body, retardedTargetState.requestedTargetNaifId, retardedTargetState.targetNaifId
        );
        correctedVector = *targetState.positionAu - *earthState.positionAu;
        lightTimeDays = correctedVector->length() / PhysicalConstants::kSpeedOfLightAuPerDay;
    }

    // The iteration that lost its target state leaves the stage unmarked, so a
    // missing or degenerate vector is reported once, here.
    if (!correctedVector.has_value() || correctedVector->length() <= std::numeric_limits<double>::min()) {
        EphemerisMetadataMerger::markCorrectionUnavailable(m_metadata, EphemerisCorrectionFlags::lightTime());
        return;
    }

    finishStage(EphemerisCorrectionFlags::lightTime(), correctedVector);
}

void SolarSystemCorrectionPipeline::applyGravitationalLightDeflection()
{
    const ICalcephKernel::StateResult sunState =
        m_kernel.compute(m_input.request.epoch, NaifBodyIds::kSun, NaifBodyIds::kEarth);
    if (!beginStage(
            EphemerisCorrectionFlags::gravitationalLightDeflection(), sunState, sunState.positionAu.has_value()
        )) {
        return;
    }

    finishStage(
        EphemerisCorrectionFlags::gravitationalLightDeflection(),
        applySolarDeflection(m_outputVector, *sunState.positionAu)
    );
}

void SolarSystemCorrectionPipeline::applyStellarAberration()
{
    const ICalcephKernel::StateResult& earthState = observerState();
    const bool observerStateComplete = earthState.positionAu.has_value() && earthState.velocityAuPerDay.has_value();
    if (!beginStage(EphemerisCorrectionFlags::stellarAberration(), earthState, observerStateComplete)) {
        return;
    }

    finishStage(
        EphemerisCorrectionFlags::stellarAberration(), applyAberration(m_outputVector, *earthState.velocityAuPerDay)
    );
}

}  // namespace skygate::ephemeris::highprecision
