#include "StarAstrometryCalculator.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "ICalcephKernelProvider.hpp"
#include "math/AngleMath.hpp"
#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/TimeConstants.hpp"
#include "math/Vector3d.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::MathConstants;
using skygate::core::PhysicalConstants;
using skygate::core::TimeConstants;
constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;

[[nodiscard]] bool isFiniteEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    return std::isfinite(coordinate.rightAscensionHours) && std::isfinite(coordinate.declinationDeg)
           && coordinate.declinationDeg >= -90.0 && coordinate.declinationDeg <= 90.0;
}

[[nodiscard]] double yearsBetween(const AstronomicalEpoch& start, const AstronomicalEpoch& end) noexcept
{
    return (end.sortKey() - start.sortKey()) / TimeConstants::kJulianDaysPerYear;
}

[[nodiscard]] bool hasPositiveParallax(const CatalogStarAstrometry& astrometry) noexcept
{
    return astrometry.stellarParallaxMas.has_value() && std::isfinite(*astrometry.stellarParallaxMas)
           && *astrometry.stellarParallaxMas > 0.0;
}

[[nodiscard]] bool
hasEnabledPositiveParallax(const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
           && hasPositiveParallax(astrometry);
}

[[nodiscard]] bool
hasAnnualParallaxInput(const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::annualParallax())
           && hasPositiveParallax(astrometry);
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

[[nodiscard]] skygate::core::Vector3d
unitVectorFromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * MathConstants::kRadiansPerHour;
    const double declinationRad = skygate::core::AngleMath::toRadians(coordinate.declinationDeg);
    const double cosDeclination = std::cos(declinationRad);
    return {
        .x = cosDeclination * std::cos(rightAscensionRad),
        .y = cosDeclination * std::sin(rightAscensionRad),
        .z = std::sin(declinationRad),
    };
}

[[nodiscard]] skygate::core::Vector3d
eastBasisFromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * MathConstants::kRadiansPerHour;
    return {
        .x = -std::sin(rightAscensionRad),
        .y = std::cos(rightAscensionRad),
        .z = 0.0,
    };
}

[[nodiscard]] skygate::core::Vector3d
northBasisFromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * MathConstants::kRadiansPerHour;
    const double declinationRad = skygate::core::AngleMath::toRadians(coordinate.declinationDeg);
    return {
        .x = -std::sin(declinationRad) * std::cos(rightAscensionRad),
        .y = -std::sin(declinationRad) * std::sin(rightAscensionRad),
        .z = std::cos(declinationRad),
    };
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
        .declinationDeg = skygate::core::AngleMath::toDegrees(std::atan2(vector.z, xyDistance)),
    };
}

[[nodiscard]] bool hasFiniteOptionalValue(const std::optional<double>& value) noexcept
{
    return value.has_value() && std::isfinite(*value);
}

[[nodiscard]] double finiteValueOrZero(const std::optional<double>& value) noexcept
{
    return hasFiniteOptionalValue(value) ? *value : 0.0;
}

[[nodiscard]] std::optional<skygate::core::Vector3d> propagatedAstrometricVector(
    const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags, const double years
) noexcept
{
    const skygate::core::EquatorialCoordinate& reference = astrometry.referenceEquatorial;
    const double properMotionRaMasPerYear =
        skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
            ? finiteValueOrZero(astrometry.properMotionRightAscensionMasPerYear)
            : 0.0;
    const double properMotionDecMasPerYear =
        skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
            ? finiteValueOrZero(astrometry.properMotionDeclinationMasPerYear)
            : 0.0;
    const double radialVelocityKmPerSecond =
        skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
            ? finiteValueOrZero(astrometry.radialVelocityKmPerSecond)
            : 0.0;

    const skygate::core::Vector3d referenceUnit = unitVectorFromEquatorial(reference);
    const skygate::core::Vector3d east = eastBasisFromEquatorial(reference);
    const skygate::core::Vector3d north = northBasisFromEquatorial(reference);
    const double tangentialRaRadiansPerYear = properMotionRaMasPerYear * MathConstants::kMilliarcsecondsToRadians;
    const double tangentialDecRadiansPerYear = properMotionDecMasPerYear * MathConstants::kMilliarcsecondsToRadians;
    const bool needsDistance =
        hasEnabledPositiveParallax(astrometry, flags) || hasAnnualParallaxInput(astrometry, flags);

    if (!needsDistance) {
        return referenceUnit + (east * (tangentialRaRadiansPerYear * years))
               + (north * (tangentialDecRadiansPerYear * years));
    }

    const double distanceAu = PhysicalConstants::kAuPerParsec * (1'000.0 / *astrometry.stellarParallaxMas);
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

[[nodiscard]] std::optional<AstronomicalEpoch> tdbEpochForKernel(
    EphemerisEngineQueryResult& metadata,
    const AstronomicalEpoch& epoch,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService
) noexcept
{
    if (epoch.timeScale == TimeScale::Tdb) {
        return epoch;
    }
    if (timeScaleService == nullptr) {
        if (metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
            metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
        }
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return std::nullopt;
    }

    const TimeScaleConversionResult conversion = timeScaleService->convert(epoch, TimeScale::Tdb);
    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);
    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return conversion.epoch;
}

[[nodiscard]] std::optional<AstronomicalEpoch> tdbEpochForKernel(
    EphemerisEngineQueryResult& metadata,
    const AstronomicalEpoch& epoch,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
    const PreparedEphemerisRequestState* preparedState
) noexcept
{
    if (preparedState == nullptr) {
        return tdbEpochForKernel(metadata, epoch, timeScaleService);
    }

    EphemerisMetadataMerger::merge(
        metadata, preparedState->tdbKernelEpochMetadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
    );
    return preparedState->tdbKernelEpoch;
}

[[nodiscard]] SolarSystemKernelStateResult earthStateForAnnualParallax(
    const AstronomicalEpoch& kernelEpoch,
    const std::shared_ptr<const ICalcephKernelProvider>& kernelProvider,
    const PreparedEphemerisRequestState* preparedState
)
{
    if (preparedState == nullptr || !preparedState->annualParallaxEarthState.has_value()) {
        return kernelProvider->computeGeometricState(kernelEpoch, kNaifEarth, kNaifSolarSystemBarycenter);
    }

    return *preparedState->annualParallaxEarthState;
}

void recordUnavailableRequestedFields(
    EphemerisEngineQueryResult& metadata, const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags
) noexcept
{
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
        && (!hasFiniteOptionalValue(astrometry.properMotionRightAscensionMasPerYear)
            || !hasFiniteOptionalValue(astrometry.properMotionDeclinationMasPerYear))) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::properMotion());
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
        && !hasPositiveParallax(astrometry)) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::stellarParallax());
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::annualParallax())
        && !hasPositiveParallax(astrometry)) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::annualParallax());
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
        && (!hasFiniteOptionalValue(astrometry.radialVelocityKmPerSecond)
            || !hasEnabledPositiveParallax(astrometry, flags))) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::radialVelocity());
    }
}

void recordAppliedCorrections(
    EphemerisEngineQueryResult& metadata, const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags
) noexcept
{
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::properMotion())
        && hasFiniteOptionalValue(astrometry.properMotionRightAscensionMasPerYear)
        && hasFiniteOptionalValue(astrometry.properMotionDeclinationMasPerYear)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::properMotion();
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::stellarParallax())
        && hasPositiveParallax(astrometry)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::stellarParallax();
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::radialVelocity())
        && hasFiniteOptionalValue(astrometry.radialVelocityKmPerSecond)
        && hasEnabledPositiveParallax(astrometry, flags)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::radialVelocity();
    }
}

[[nodiscard]] HighPrecisionCalculatorResult calculateStarAstrometry(
    const EphemerisRequest& request,
    const std::optional<CatalogStarAstrometry>& astrometry,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial,
    const std::shared_ptr<const ICalcephKernelProvider>& kernelProvider,
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
    if (referenceEquatorial == nullptr || !isFiniteEquatorial(*referenceEquatorial)) {
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
        if (kernelProvider == nullptr) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::annualParallax()
            );
            result.equatorial = equatorialFromVector(*propagatedVector);
        } else {
            const std::optional<AstronomicalEpoch> kernelEpoch =
                tdbEpochForKernel(result.metadata, request.epoch, timeScaleService, preparedState);
            if (!kernelEpoch.has_value()) {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::annualParallax()
                );
                result.equatorial = equatorialFromVector(*propagatedVector);
            } else {
                const SolarSystemKernelStateResult earthState =
                    earthStateForAnnualParallax(*kernelEpoch, kernelProvider, preparedState);
                EphemerisMetadataMerger::merge(
                    result.metadata, earthState.metadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
                );
                if (earthState.positionAu.has_value()) {
                    const skygate::core::Vector3d geocentricVector = *propagatedVector - *earthState.positionAu;
                    result.observerRelativePositionAu = geocentricVector;
                    result.equatorial = equatorialFromVector(geocentricVector);
                    result.metadata.appliedCorrections |= EphemerisCorrectionFlags::annualParallax();
                } else {
                    result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
                    EphemerisMetadataMerger::markCorrectionUnavailable(
                        result.metadata, EphemerisCorrectionFlags::annualParallax()
                    );
                    result.equatorial = equatorialFromVector(*propagatedVector);
                }
            }
        }
    } else {
        result.equatorial = equatorialFromVector(*propagatedVector);
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
    std::shared_ptr<const ICalcephKernelProvider> kernelProvider,
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService
)
    : m_kernelProvider(std::move(kernelProvider)), m_timeScaleService(std::move(timeScaleService))
{
}

HighPrecisionCalculatorResult StarAstrometryCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    return calculateStarAstrometry(
        input.request,
        input.body.starAstrometryValue(),
        input.body.fixedEquatorialValue(),
        m_kernelProvider,
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
    std::shared_ptr<PreparedEphemerisRequestState> localPreparedState;
    if (preparedRequestState == nullptr
        && skygate::ephemeris::EphemerisCorrectionFlags::has(
            request.options.correctionFlags(), EphemerisCorrectionFlags::annualParallax()
        )) {
        localPreparedState = std::make_shared<PreparedEphemerisRequestState>();
        localPreparedState->tdbKernelEpoch =
            tdbEpochForKernel(localPreparedState->tdbKernelEpochMetadata, request.epoch, m_timeScaleService);
        if (localPreparedState->tdbKernelEpoch.has_value() && m_kernelProvider != nullptr) {
            localPreparedState->annualParallaxEarthState = m_kernelProvider->computeGeometricState(
                *localPreparedState->tdbKernelEpoch, kNaifEarth, kNaifSolarSystemBarycenter
            );
        }
        preparedRequestState = localPreparedState;
    }

    for (std::size_t arrayIndex = 0U; arrayIndex < arrays.size(); ++arrayIndex) {
        const std::optional<CatalogStarAstrometry> astrometry = astrometryFromArrays(arrays, arrayIndex);
        const std::optional<skygate::core::EquatorialCoordinate> fixedEquatorial =
            arrays.fixedEquatorialFallback(arrayIndex);
        results.push_back(
            StarAstrometryBatchResult{
                .bodyIndex = arrays.bodyIndices()[arrayIndex],
                .result = calculateStarAstrometry(
                    request,
                    astrometry,
                    fixedEquatorial,
                    m_kernelProvider,
                    m_timeScaleService,
                    preparedRequestState.get()
                ),
            }
        );
    }

    return results;
}

}  // namespace skygate::ephemeris::highprecision
