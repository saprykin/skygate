#include "StarAstrometryCalculator.hpp"
#include "CelestialFrameMath.hpp"
#include "EphemerisMetadataFailurePolicy.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "EpochScaleResolver.hpp"
#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "ICalcephKernel.hpp"
#include "NaifBodyIds.hpp"
#include "PreparedEphemerisRequestState.hpp"
#include "PreparedRequestStateBuilder.hpp"
#include "StarAstrometryBatchResult.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "engine/ITimeScaleService.hpp"
#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/TimeConstants.hpp"
#include "math/Vector3d.hpp"

#include <memory>
#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::MathConstants;
using skygate::core::NaifBodyIds;
using skygate::core::PhysicalConstants;
using skygate::core::TimeConstants;

[[nodiscard]] double
yearsBetween(const skygate::core::AstronomicalEpoch& start, const skygate::core::AstronomicalEpoch& end) noexcept
{
    return (end.sortKey() - start.sortKey()) / TimeConstants::kJulianDaysPerYear;
}

[[nodiscard]] bool
hasEnabledPositiveParallax(const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
           && astrometry.hasUsableParallax();
}

[[nodiscard]] bool
hasAnnualParallaxInput(const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::annualParallax())
           && astrometry.hasUsableParallax();
}

[[nodiscard]] bool requestsAnyAstrometryCorrection(const EphemerisCorrectionFlags flags) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
           || skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
           || skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
           || skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::annualParallax());
}

[[nodiscard]] HighPrecisionCalculatorResult makeFailedResult() noexcept
{
    HighPrecisionCalculatorResult result;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    result.metadata.dataSourceProvenance = "catalog star astrometry";
    return result;
}

[[nodiscard]] std::optional<skygate::core::Vector3d> propagatedAstrometricVector(
    const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags, const double years
) noexcept
{
    const skygate::core::EquatorialCoordinate& reference = astrometry.referenceEquatorial;
    const double properMotionRaMasPerYear =
        skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
                && astrometry.hasUsableProperMotionRightAscension()
            ? *astrometry.properMotionRightAscensionMasPerYear
            : 0.0;
    const double properMotionDecMasPerYear =
        skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
                && astrometry.hasUsableProperMotionDeclination()
            ? *astrometry.properMotionDeclinationMasPerYear
            : 0.0;
    const double radialVelocityKmPerSecond =
        skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
                && astrometry.hasUsableRadialVelocity()
            ? *astrometry.radialVelocityKmPerSecond
            : 0.0;

    const skygate::core::Vector3d referenceUnit = CelestialFrameMath::fromEquatorial(reference);
    const skygate::core::Vector3d east = CelestialFrameMath::eastBasisFromEquatorial(reference);
    const skygate::core::Vector3d north = CelestialFrameMath::northBasisFromEquatorial(reference);
    const double tangentialRaRadiansPerYear = properMotionRaMasPerYear * MathConstants::kMilliarcsecondsToRadians;
    const double tangentialDecRadiansPerYear = properMotionDecMasPerYear * MathConstants::kMilliarcsecondsToRadians;
    const bool needsDistance =
        hasEnabledPositiveParallax(astrometry, flags) || hasAnnualParallaxInput(astrometry, flags);

    if (!needsDistance) {
        return referenceUnit + (east * (tangentialRaRadiansPerYear * years))
               + (north * (tangentialDecRadiansPerYear * years));
    }

    const double distanceAu = PhysicalConstants::kAuPerParsec
                              * CatalogStarAstrometry::distanceParsecsFromParallaxMas(*astrometry.stellarParallaxMas);
    const double radialVelocityAuPerYear = hasEnabledPositiveParallax(astrometry, flags)
                                               ? radialVelocityKmPerSecond * TimeConstants::kSecondsPerJulianYear
                                                     / PhysicalConstants::kAstronomicalUnitKilometers
                                               : 0.0;
    const skygate::core::Vector3d referencePosition = referenceUnit * distanceAu;
    const skygate::core::Vector3d velocity = (referenceUnit * radialVelocityAuPerYear)
                                             + (east * (tangentialRaRadiansPerYear * distanceAu))
                                             + (north * (tangentialDecRadiansPerYear * distanceAu));
    return referencePosition + (velocity * years);
}

[[nodiscard]] std::optional<skygate::core::AstronomicalEpoch> kernelEpochForAnnualParallax(
    EphemerisEngineQueryResult& metadata,
    const skygate::core::AstronomicalEpoch& epoch,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
    const PreparedEphemerisRequestState* preparedState
) noexcept
{
    if (preparedState == nullptr) {
        return EpochScaleResolver::resolve(
            metadata, epoch, timeScaleService, EphemerisMetadataFailurePolicy::MarkDegraded
        );
    }

    EphemerisMetadataMerger::merge(
        metadata, preparedState->tdbKernelEpochMetadata, EphemerisMetadataMergeOptions::withoutCorrections()
    );
    return preparedState->tdbKernelEpoch;
}

[[nodiscard]] ICalcephKernel::StateResult earthStateForAnnualParallax(
    const skygate::core::AstronomicalEpoch& kernelEpoch,
    const std::shared_ptr<const ICalcephKernel>& kernel,
    const PreparedEphemerisRequestState* preparedState
)
{
    if (preparedState == nullptr || !preparedState->annualParallaxEarthState.has_value()) {
        return kernel->compute(kernelEpoch, NaifBodyIds::kEarth, NaifBodyIds::kSolarSystemBarycenter);
    }

    return *preparedState->annualParallaxEarthState;
}

void recordUnavailableRequestedFields(
    EphemerisEngineQueryResult& metadata, const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags
) noexcept
{
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
        && (!astrometry.hasUsableProperMotionRightAscension() || !astrometry.hasUsableProperMotionDeclination())) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::properMotion());
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
        && !astrometry.hasUsableParallax()) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::stellarParallax());
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::annualParallax())
        && !astrometry.hasUsableParallax()) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::annualParallax());
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
        && (!astrometry.hasUsableRadialVelocity() || !hasEnabledPositiveParallax(astrometry, flags))) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::radialVelocity());
    }
}

void recordAppliedCorrections(
    EphemerisEngineQueryResult& metadata, const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags
) noexcept
{
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
        && astrometry.hasUsableProperMotionRightAscension() && astrometry.hasUsableProperMotionDeclination()) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::properMotion();
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
        && astrometry.hasUsableParallax()) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::stellarParallax();
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
        && astrometry.hasUsableRadialVelocity() && hasEnabledPositiveParallax(astrometry, flags)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::radialVelocity();
    }
}

[[nodiscard]] HighPrecisionCalculatorResult calculateStarAstrometry(
    const EphemerisRequest& request,
    const std::optional<CatalogStarAstrometry>& astrometry,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial,
    const std::shared_ptr<const ICalcephKernel>& kernel,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
    const PreparedEphemerisRequestState* preparedState = nullptr
)
{
    const EphemerisCorrectionFlags flags = request.options.correctionFlags();
    const skygate::core::EquatorialCoordinate* referenceEquatorial = nullptr;
    if (astrometry.has_value()) {
        referenceEquatorial = &astrometry->referenceEquatorial;
    } else if (fixedEquatorial.has_value()) {
        referenceEquatorial = &*fixedEquatorial;
    }
    if (referenceEquatorial == nullptr || !referenceEquatorial->isValid()) {
        return makeFailedResult();
    }

    HighPrecisionCalculatorResult result;
    result.equatorial = *referenceEquatorial;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.dataSourceProvenance =
        astrometry.has_value() ? "catalog star astrometry" : "fixed catalog coordinates";
    if (astrometry.has_value() && astrometry->validityRange.has_value()) {
        result.metadata.effectiveDataValidityRange = astrometry->validityRange;
    }

    if (!astrometry.has_value()) {
        if (requestsAnyAstrometryCorrection(flags)) {
            result.metadata.addUnavailableCorrection(
                flags
                & (EphemerisCorrectionFlags::properMotion() | EphemerisCorrectionFlags::radialVelocity()
                   | EphemerisCorrectionFlags::stellarParallax() | EphemerisCorrectionFlags::annualParallax())
            );
            if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
                result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
            }
        }
        return result;
    }

    if (!request.epoch.isFinite() || !astrometry->referenceEpoch.isFinite()) {
        return makeFailedResult();
    }

    const double elapsedYears = yearsBetween(astrometry->referenceEpoch, request.epoch);
    const std::optional<skygate::core::Vector3d> propagatedVector =
        propagatedAstrometricVector(*astrometry, flags, elapsedYears);
    if (!propagatedVector.has_value()) {
        return makeFailedResult();
    }

    if (hasAnnualParallaxInput(*astrometry, flags)) {
        if (kernel == nullptr) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::annualParallax()
            );
            result.equatorial = CelestialFrameMath::toEquatorial(*propagatedVector);
        } else {
            const std::optional<skygate::core::AstronomicalEpoch> kernelEpoch =
                kernelEpochForAnnualParallax(result.metadata, request.epoch, timeScaleService, preparedState);
            if (!kernelEpoch.has_value()) {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::annualParallax()
                );
                result.equatorial = CelestialFrameMath::toEquatorial(*propagatedVector);
            } else {
                const ICalcephKernel::StateResult earthState =
                    earthStateForAnnualParallax(*kernelEpoch, kernel, preparedState);
                EphemerisMetadataMerger::merge(
                    result.metadata, earthState.metadata, EphemerisMetadataMergeOptions::withoutCorrections()
                );
                if (earthState.positionAu.has_value()) {
                    const skygate::core::Vector3d geocentricVector = *propagatedVector - *earthState.positionAu;
                    result.observerRelativePositionAu = geocentricVector;
                    result.equatorial = CelestialFrameMath::toEquatorial(geocentricVector);
                    result.metadata.appliedCorrections |= EphemerisCorrectionFlags::annualParallax();
                } else {
                    EphemerisMetadataMerger::mergeKernelDiagnostics(result.metadata, *kernel);
                    result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
                    EphemerisMetadataMerger::markCorrectionUnavailable(
                        result.metadata, EphemerisCorrectionFlags::annualParallax()
                    );
                    result.equatorial = CelestialFrameMath::toEquatorial(*propagatedVector);
                }
            }
        }
    } else {
        result.equatorial = CelestialFrameMath::toEquatorial(*propagatedVector);
    }
    if (!result.equatorial.has_value()) {
        return makeFailedResult();
    }
    recordUnavailableRequestedFields(result.metadata, *astrometry, flags);
    recordAppliedCorrections(result.metadata, *astrometry, flags);
    return result;
}

[[nodiscard]] std::optional<CatalogStarAstrometry>
astrometryFromArrays(const CatalogStarAstrometryArrays& arrays, const std::size_t arrayIndex)
{
    if (!arrays.hasCatalogAstrometry(arrayIndex)) {
        return std::nullopt;
    }

    return CatalogStarAstrometry{
        .referenceEquatorial = arrays.referenceEquatorial(arrayIndex),
        .referenceEpoch = arrays.referenceEpoch(arrayIndex),
        .properMotionRightAscensionMasPerYear = arrays.properMotionRightAscensionMasPerYear(arrayIndex),
        .properMotionDeclinationMasPerYear = arrays.properMotionDeclinationMasPerYear(arrayIndex),
        .stellarParallaxMas = arrays.stellarParallaxMas(arrayIndex),
        .radialVelocityKmPerSecond = arrays.radialVelocityKmPerSecond(arrayIndex),
        .validityRange = arrays.validityRange(arrayIndex),
    };
}

}  // namespace

StarAstrometryCalculator::StarAstrometryCalculator(
    std::shared_ptr<const ICalcephKernel> kernel,
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService
)
    : m_kernel(std::move(kernel)), m_timeScaleService(std::move(timeScaleService))
{
}

HighPrecisionCalculatorResult StarAstrometryCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    return calculateStarAstrometry(
        input.request,
        input.body.starAstrometryValue(),
        input.body.fixedEquatorialValue(),
        m_kernel,
        m_timeScaleService,
        input.preparedRequestState.get()
    );
}

std::vector<StarAstrometryBatchResult> StarAstrometryCalculator::calculateBatch(
    const EphemerisRequest& request,
    const CatalogStarAstrometryArrays& arrays,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    std::vector<StarAstrometryBatchResult> results;
    results.reserve(arrays.size());
    if (preparedRequestState == nullptr
        && skygate::ephemeris::EphemerisCorrectionFlags::has(
            request.options.correctionFlags(), EphemerisCorrectionFlags::annualParallax()
        )) {
        const PreparedRequestStateBuilder builder(
            PreparedRequestStateBuilder::Dependencies{
                .calcephKernel = m_kernel,
                .timeScaleService = m_timeScaleService,
                .earthOrientationProvider = nullptr,
            }
        );
        preparedRequestState = builder.build(request);
    }

    for (std::size_t arrayIndex = 0U; arrayIndex < arrays.size(); ++arrayIndex) {
        const std::optional<CatalogStarAstrometry> astrometry = astrometryFromArrays(arrays, arrayIndex);
        const std::optional<skygate::core::EquatorialCoordinate> fixedEquatorial =
            arrays.fixedEquatorialFallback(arrayIndex);
        results.push_back(
            StarAstrometryBatchResult{
                .bodyIndex = arrays.bodyIndices()[arrayIndex],
                .result = calculateStarAstrometry(
                    request, astrometry, fixedEquatorial, m_kernel, m_timeScaleService, preparedRequestState.get()
                ),
            }
        );
    }

    return results;
}

}  // namespace skygate::ephemeris::highprecision
