#include "SolarSystemStateCalculator.hpp"
#include "CelestialFrameMath.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "ICalcephKernel.hpp"
#include "NaifBodyIds.hpp"
#include "PlanetIdentity.hpp"
#include "SolarSystemCorrectionPipeline.hpp"
#include "math/Vector3d.hpp"

#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::NaifBodyIds;

[[nodiscard]] std::optional<int> naifIdForBody(const BaseCelestialBody& body) noexcept
{
    if (body.kind == BaseCelestialBody::Kind::Sun) {
        return NaifBodyIds::kSun;
    }
    if (body.kind == BaseCelestialBody::Kind::Moon) {
        return NaifBodyIds::kMoon;
    }
    if (body.kind != BaseCelestialBody::Kind::Planet) {
        return std::nullopt;
    }

    if (const std::optional<PlanetIdentity> planet = PlanetIdentity::fromBodyId(body.id); planet.has_value()) {
        return planet->naifBodyCenterId();
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<int> planetarySystemBarycenterNaifIdForBody(const BaseCelestialBody& body) noexcept
{
    if (body.kind != BaseCelestialBody::Kind::Planet) {
        return std::nullopt;
    }

    if (const std::optional<PlanetIdentity> planet = PlanetIdentity::fromBodyId(body.id); planet.has_value()) {
        return planet->naifBarycenterId();
    }
    return std::nullopt;
}

[[nodiscard]] bool de440sHasPlanetBodyCenter(const BaseCelestialBody& body) noexcept
{
    const std::optional<PlanetIdentity> planet = PlanetIdentity::fromBodyId(body.id);
    return planet.has_value() && planet->hasDe440sBodyCenter();
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

[[nodiscard]] HighPrecisionCalculatorResult
makeStatusResult(const EphemerisEngineQueryStatus::Type status, const EphemerisEngineWarning::Code warningCode)
{
    HighPrecisionCalculatorResult result;
    result.metadata.status = status;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    return result;
}

}  // namespace

SolarSystemStateCalculator::SolarSystemStateCalculator(
    std::shared_ptr<const ICalcephKernel> kernel, const bool preferPlanetarySystemBarycenters
)
    : m_kernel(std::move(kernel)), m_preferPlanetarySystemBarycenters(preferPlanetarySystemBarycenters)
{
}

HighPrecisionCalculatorResult SolarSystemStateCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    if (!input.request.epoch.isFinite()) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Failed, EphemerisEngineWarning::Code::ComputationFailed
        );
    }
    if (input.request.epoch.timeScale != skygate::core::TimeScale::Tdb) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Failed, EphemerisEngineWarning::Code::TimeScaleDataUnavailable
        );
    }
    if (m_kernel == nullptr) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Failed, EphemerisEngineWarning::Code::MissingEphemerisData
        );
    }

    const std::optional<int> targetNaifId = naifIdForBody(input.body);
    const std::optional<int> fallbackTargetNaifId = planetarySystemBarycenterNaifIdForBody(input.body);
    const bool preferPlanetarySystemBarycenter =
        shouldPreferPlanetarySystemBarycenter(input.body, m_preferPlanetarySystemBarycenters);
    if (!targetNaifId.has_value() || *targetNaifId == NaifBodyIds::kEarth) {
        return makeStatusResult(
            EphemerisEngineQueryStatus::Type::Unsupported, EphemerisEngineWarning::Code::UnsupportedBody
        );
    }

    const SolarSystemCorrectionPipeline::TargetKernelState targetKernelResult =
        SolarSystemCorrectionPipeline::computeTargetKernelState(
            *m_kernel,
            input.request.epoch,
            *targetNaifId,
            fallbackTargetNaifId,
            NaifBodyIds::kEarth,
            preferPlanetarySystemBarycenter
        );
    const ICalcephKernel::StateResult& kernelResult = targetKernelResult.state;
    const int effectiveTargetNaifId = targetKernelResult.targetNaifId;
    HighPrecisionCalculatorResult result;
    result.metadata = kernelResult.metadata;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::geometric();
    if (result.metadata.dataSourceProvenance.empty()) {
        result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    }
    SolarSystemCorrectionPipeline::markBarycenterFallback(
        result.metadata, input.body, targetKernelResult.requestedTargetNaifId, effectiveTargetNaifId
    );

    if (!kernelResult.positionAu.has_value()) {
        EphemerisMetadataMerger::mergeKernelDiagnostics(result.metadata, *m_kernel);
        if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        }
        return result;
    }

    SolarSystemCorrectionPipeline pipeline(
        *m_kernel,
        input,
        result.metadata,
        *kernelResult.positionAu,
        effectiveTargetNaifId,
        fallbackTargetNaifId,
        preferPlanetarySystemBarycenter
    );
    pipeline.applyCorrections(input.request.options.correctionFlags());

    const skygate::core::Vector3d outputVector = pipeline.observerRelativePositionAu();
    result.observerRelativePositionAu = outputVector;
    result.equatorial = CelestialFrameMath::toEquatorial(outputVector);
    if (!result.equatorial.has_value()) {
        result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
