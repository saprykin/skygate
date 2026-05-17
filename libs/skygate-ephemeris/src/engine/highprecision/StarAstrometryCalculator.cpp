#include "engine/highprecision/StarAstrometryCalculator.hpp"

#include "engine/highprecision/EphemerisMetadataMerge.hpp"
#include "skygate/core/math/AngleMath.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kRadiansPerHour = kPi / 12.0;
constexpr double kHoursPerRadian = 12.0 / kPi;
constexpr double kMasToRadians = kPi / (180.0 * 3'600'000.0);
constexpr double kJulianDaysPerYear = 365.25;
constexpr double kAuPerParsec = 206'264.80624709636;
constexpr double kAuKilometers = 149'597'870.7;
constexpr double kSecondsPerJulianYear = 31'557'600.0;
constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;

struct CartesianVector {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] bool isFiniteEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
{
    return std::isfinite(coordinate.rightAscensionHours) && std::isfinite(coordinate.declinationDeg)
           && coordinate.declinationDeg >= -90.0 && coordinate.declinationDeg <= 90.0;
}

[[nodiscard]] double epochSortKey(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalized = normalizedAstronomicalEpoch(epoch);
    return normalized.julianDatePart1 + normalized.julianDatePart2;
}

[[nodiscard]] double yearsBetween(const AstronomicalEpoch& start, const AstronomicalEpoch& end) noexcept
{
    return (epochSortKey(end) - epochSortKey(start)) / kJulianDaysPerYear;
}

[[nodiscard]] bool hasPositiveParallax(const CatalogStarAstrometry& astrometry) noexcept
{
    return astrometry.stellarParallaxMas.has_value() && std::isfinite(*astrometry.stellarParallaxMas)
           && *astrometry.stellarParallaxMas > 0.0;
}

[[nodiscard]] bool
hasEnabledPositiveParallax(const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags) noexcept
{
    return hasCorrectionFlag(flags, EphemerisCorrectionFlags::StellarParallax) && hasPositiveParallax(astrometry);
}

[[nodiscard]] bool
hasAnnualParallaxInput(const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags) noexcept
{
    return hasCorrectionFlag(flags, EphemerisCorrectionFlags::AnnualParallax) && hasPositiveParallax(astrometry);
}

[[nodiscard]] bool requestsAnyAstrometryCorrection(const EphemerisCorrectionFlags flags) noexcept
{
    return hasCorrectionFlag(flags, EphemerisCorrectionFlags::ProperMotion)
           || hasCorrectionFlag(flags, EphemerisCorrectionFlags::RadialVelocity)
           || hasCorrectionFlag(flags, EphemerisCorrectionFlags::StellarParallax)
           || hasCorrectionFlag(flags, EphemerisCorrectionFlags::AnnualParallax);
}

[[nodiscard]] HighPrecisionCalculatorResult makeFailedResult() noexcept
{
    HighPrecisionCalculatorResult result;
    result.metadata.status = EphemerisResultStatus::Failed;
    result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
    result.metadata.dataSourceProvenance = "catalog star astrometry";
    return result;
}

[[nodiscard]] CartesianVector addVectors(const CartesianVector& lhs, const CartesianVector& rhs) noexcept
{
    return {
        .x = lhs.x + rhs.x,
        .y = lhs.y + rhs.y,
        .z = lhs.z + rhs.z,
    };
}

[[nodiscard]] CartesianVector subtractVectors(const CartesianVector& lhs, const CartesianVector& rhs) noexcept
{
    return {
        .x = lhs.x - rhs.x,
        .y = lhs.y - rhs.y,
        .z = lhs.z - rhs.z,
    };
}

[[nodiscard]] CartesianVector scaleVector(const CartesianVector& vector, const double scale) noexcept
{
    return {
        .x = vector.x * scale,
        .y = vector.y * scale,
        .z = vector.z * scale,
    };
}

[[nodiscard]] CartesianVector unitVectorFromEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * kRadiansPerHour;
    const double declinationRad = core::AngleMath::toRadians(coordinate.declinationDeg);
    const double cosDeclination = std::cos(declinationRad);
    return {
        .x = cosDeclination * std::cos(rightAscensionRad),
        .y = cosDeclination * std::sin(rightAscensionRad),
        .z = std::sin(declinationRad),
    };
}

[[nodiscard]] CartesianVector eastBasisFromEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * kRadiansPerHour;
    return {
        .x = -std::sin(rightAscensionRad),
        .y = std::cos(rightAscensionRad),
        .z = 0.0,
    };
}

[[nodiscard]] CartesianVector northBasisFromEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * kRadiansPerHour;
    const double declinationRad = core::AngleMath::toRadians(coordinate.declinationDeg);
    return {
        .x = -std::sin(declinationRad) * std::cos(rightAscensionRad),
        .y = -std::sin(declinationRad) * std::sin(rightAscensionRad),
        .z = std::cos(declinationRad),
    };
}

[[nodiscard]] std::optional<core::EquatorialCoordinate> equatorialFromVector(const CartesianVector& vector) noexcept
{
    if (!std::isfinite(vector.x) || !std::isfinite(vector.y) || !std::isfinite(vector.z)) {
        return std::nullopt;
    }

    const double xyDistance = std::hypot(vector.x, vector.y);
    const double distance = std::hypot(xyDistance, vector.z);
    if (distance <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    double rightAscensionHours = std::atan2(vector.y, vector.x) * kHoursPerRadian;
    if (rightAscensionHours < 0.0) {
        rightAscensionHours += 24.0;
    }

    return core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = core::AngleMath::toDegrees(std::atan2(vector.z, xyDistance)),
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

[[nodiscard]] std::optional<CartesianVector> propagatedAstrometricVector(
    const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags, const double years
) noexcept
{
    const core::EquatorialCoordinate& reference = astrometry.referenceEquatorial;
    const double properMotionRaMasPerYear = hasCorrectionFlag(flags, EphemerisCorrectionFlags::ProperMotion)
                                                ? finiteValueOrZero(astrometry.properMotionRightAscensionMasPerYear)
                                                : 0.0;
    const double properMotionDecMasPerYear = hasCorrectionFlag(flags, EphemerisCorrectionFlags::ProperMotion)
                                                 ? finiteValueOrZero(astrometry.properMotionDeclinationMasPerYear)
                                                 : 0.0;
    const double radialVelocityKmPerSecond = hasCorrectionFlag(flags, EphemerisCorrectionFlags::RadialVelocity)
                                                 ? finiteValueOrZero(astrometry.radialVelocityKmPerSecond)
                                                 : 0.0;

    const CartesianVector referenceUnit = unitVectorFromEquatorial(reference);
    const CartesianVector east = eastBasisFromEquatorial(reference);
    const CartesianVector north = northBasisFromEquatorial(reference);
    const double tangentialRaRadiansPerYear = properMotionRaMasPerYear * kMasToRadians;
    const double tangentialDecRadiansPerYear = properMotionDecMasPerYear * kMasToRadians;
    const bool needsDistance =
        hasEnabledPositiveParallax(astrometry, flags) || hasAnnualParallaxInput(astrometry, flags);

    if (!needsDistance) {
        return addVectors(
            referenceUnit,
            addVectors(
                scaleVector(east, tangentialRaRadiansPerYear * years),
                scaleVector(north, tangentialDecRadiansPerYear * years)
            )
        );
    }

    const double distanceAu = kAuPerParsec * (1'000.0 / *astrometry.stellarParallaxMas);
    const double radialVelocityAuPerYear = hasEnabledPositiveParallax(astrometry, flags)
                                               ? radialVelocityKmPerSecond * kSecondsPerJulianYear / kAuKilometers
                                               : 0.0;
    const CartesianVector referencePosition = scaleVector(referenceUnit, distanceAu);
    const CartesianVector velocity = addVectors(
        scaleVector(referenceUnit, radialVelocityAuPerYear),
        addVectors(
            scaleVector(east, tangentialRaRadiansPerYear * distanceAu),
            scaleVector(north, tangentialDecRadiansPerYear * distanceAu)
        )
    );
    return addVectors(referencePosition, scaleVector(velocity, years));
}

[[nodiscard]] CartesianVector cartesianFromSolarSystemVector(const SolarSystemKernelVector& vector) noexcept
{
    return {
        .x = vector.xAu,
        .y = vector.yAu,
        .z = vector.zAu,
    };
}

[[nodiscard]] SolarSystemKernelVector solarSystemVectorFromCartesian(const CartesianVector& vector) noexcept
{
    return {
        .xAu = vector.x,
        .yAu = vector.y,
        .zAu = vector.z,
    };
}

[[nodiscard]] std::optional<AstronomicalEpoch> tdbEpochForKernel(
    EphemerisResultMetadata& metadata,
    const AstronomicalEpoch& epoch,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService
) noexcept
{
    if (epoch.timeScale == TimeScale::Tdb) {
        return epoch;
    }
    if (timeScaleService == nullptr) {
        if (metadata.status == EphemerisResultStatus::Valid) {
            metadata.status = EphemerisResultStatus::Degraded;
        }
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
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
    EphemerisResultMetadata& metadata,
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
    EphemerisResultMetadata& metadata, const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags
) noexcept
{
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::ProperMotion)
        && (!hasFiniteOptionalValue(astrometry.properMotionRightAscensionMasPerYear)
            || !hasFiniteOptionalValue(astrometry.properMotionDeclinationMasPerYear))) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::ProperMotion);
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::StellarParallax) && !hasPositiveParallax(astrometry)) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::StellarParallax);
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::AnnualParallax) && !hasPositiveParallax(astrometry)) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::AnnualParallax);
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::RadialVelocity)
        && (!hasFiniteOptionalValue(astrometry.radialVelocityKmPerSecond)
            || !hasEnabledPositiveParallax(astrometry, flags))) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::RadialVelocity);
    }
}

void recordAppliedCorrections(
    EphemerisResultMetadata& metadata, const CatalogStarAstrometry& astrometry, const EphemerisCorrectionFlags flags
) noexcept
{
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::ProperMotion)
        && hasFiniteOptionalValue(astrometry.properMotionRightAscensionMasPerYear)
        && hasFiniteOptionalValue(astrometry.properMotionDeclinationMasPerYear)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::ProperMotion;
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::StellarParallax) && hasPositiveParallax(astrometry)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::StellarParallax;
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::RadialVelocity)
        && hasFiniteOptionalValue(astrometry.radialVelocityKmPerSecond)
        && hasEnabledPositiveParallax(astrometry, flags)) {
        metadata.appliedCorrections |= EphemerisCorrectionFlags::RadialVelocity;
    }
}

[[nodiscard]] HighPrecisionCalculatorResult calculateStarAstrometry(
    const EphemerisRequest& request,
    const std::optional<CatalogStarAstrometry>& astrometry,
    const std::optional<core::EquatorialCoordinate>& fixedEquatorial,
    const std::shared_ptr<const ICalcephKernelProvider>& kernelProvider,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
    const PreparedEphemerisRequestState* preparedState = nullptr
)
{
    const EphemerisCorrectionFlags flags = request.options.correctionFlags;
    const core::EquatorialCoordinate* referenceEquatorial = nullptr;
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
    result.metadata.status = EphemerisResultStatus::Valid;
    result.metadata.dataSourceProvenance =
        astrometry.has_value() ? "catalog star astrometry" : "fixed catalog coordinates";
    if (astrometry.has_value() && astrometry->validityRange.has_value()) {
        result.metadata.effectiveDataValidityRange = astrometry->validityRange;
    }

    if (!astrometry.has_value()) {
        if (requestsAnyAstrometryCorrection(flags)) {
            result.metadata.addUnavailableCorrection(
                flags
                & (EphemerisCorrectionFlags::ProperMotion | EphemerisCorrectionFlags::RadialVelocity
                   | EphemerisCorrectionFlags::StellarParallax | EphemerisCorrectionFlags::AnnualParallax)
            );
            if (result.metadata.status == EphemerisResultStatus::Valid) {
                result.metadata.status = EphemerisResultStatus::Degraded;
            }
        }
        return result;
    }

    if (!isFiniteEpoch(request.epoch) || !isFiniteEpoch(astrometry->referenceEpoch)) {
        return makeFailedResult();
    }

    const double elapsedYears = yearsBetween(astrometry->referenceEpoch, request.epoch);
    const std::optional<CartesianVector> propagatedVector =
        propagatedAstrometricVector(*astrometry, flags, elapsedYears);
    if (!propagatedVector.has_value()) {
        return makeFailedResult();
    }

    if (hasAnnualParallaxInput(*astrometry, flags)) {
        if (kernelProvider == nullptr) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::AnnualParallax
            );
            result.equatorial = equatorialFromVector(*propagatedVector);
        } else {
            const std::optional<AstronomicalEpoch> kernelEpoch =
                tdbEpochForKernel(result.metadata, request.epoch, timeScaleService, preparedState);
            if (!kernelEpoch.has_value()) {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::AnnualParallax
                );
                result.equatorial = equatorialFromVector(*propagatedVector);
            } else {
                const SolarSystemKernelStateResult earthState =
                    earthStateForAnnualParallax(*kernelEpoch, kernelProvider, preparedState);
                EphemerisMetadataMerger::merge(
                    result.metadata, earthState.metadata, EphemerisMetadataMergeOptions{.mergeCorrections = false}
                );
                if (earthState.positionAu.has_value()) {
                    const CartesianVector geocentricVector =
                        subtractVectors(*propagatedVector, cartesianFromSolarSystemVector(*earthState.positionAu));
                    result.observerRelativePositionAu = solarSystemVectorFromCartesian(geocentricVector);
                    result.equatorial = equatorialFromVector(geocentricVector);
                    result.metadata.appliedCorrections |= EphemerisCorrectionFlags::AnnualParallax;
                } else {
                    result.metadata.status = EphemerisResultStatus::Degraded;
                    EphemerisMetadataMerger::markCorrectionUnavailable(
                        result.metadata, EphemerisCorrectionFlags::AnnualParallax
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
        input.body.starAstrometry,
        input.body.fixedEquatorial,
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
        && hasCorrectionFlag(request.options.correctionFlags, EphemerisCorrectionFlags::AnnualParallax)) {
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
        const std::optional<core::EquatorialCoordinate> fixedEquatorial = arrays.fixedEquatorialFallback(arrayIndex);
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
