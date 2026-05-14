#include "engine/highprecision/ApparentPlaceCalculator.hpp"

#include "engine/highprecision/FrameTransformer.hpp"
#include "skygate/core/math/AngleMath.hpp"
#include "skygate/ephemeris/EarthOrientationProvider.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr double kRadiansPerHour = 3.141592653589793238462643383279502884 / 12.0;
constexpr double kHoursPerRadian = 12.0 / 3.141592653589793238462643383279502884;
constexpr double kAstronomicalUnitMeters = 149'597'870'700.0;
constexpr double kWgs84EquatorialRadiusMeters = 6'378'137.0;
constexpr double kWgs84Flattening = 1.0 / 298.257223563;

enum class ApparentPlaceRequestMode : std::uint8_t {
    Geometric,
    Astrometric,
    Apparent,
    Topocentric
};

[[nodiscard]] bool isFiniteEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
{
    return std::isfinite(coordinate.rightAscensionHours) && std::isfinite(coordinate.declinationDeg);
}

[[nodiscard]] ApparentPlaceRequestMode requestModeForCorrections(const EphemerisCorrectionFlags flags) noexcept
{
    if (flags == EphemerisCorrectionFlags::NoCorrections) {
        return ApparentPlaceRequestMode::Geometric;
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::DiurnalParallax)) {
        return ApparentPlaceRequestMode::Topocentric;
    }
    if (hasCorrectionFlag(flags, EphemerisCorrectionFlags::PrecessionNutation)) {
        return ApparentPlaceRequestMode::Apparent;
    }

    return ApparentPlaceRequestMode::Astrometric;
}

[[nodiscard]] CelestialReferenceFrame targetFrameForRequest(const ApparentPlaceRequestMode mode) noexcept
{
    switch (mode) {
    case ApparentPlaceRequestMode::Geometric:
    case ApparentPlaceRequestMode::Astrometric:
        return CelestialReferenceFrame::Gcrs;
    case ApparentPlaceRequestMode::Apparent:
        return CelestialReferenceFrame::TrueEquatorAndEquinox;
    case ApparentPlaceRequestMode::Topocentric:
        return CelestialReferenceFrame::Itrs;
    }

    return CelestialReferenceFrame::Gcrs;
}

[[nodiscard]] CelestialFrameVector vectorFromEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
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

[[nodiscard]] bool isFiniteSolarSystemVector(const SolarSystemKernelVector& vector) noexcept
{
    return std::isfinite(vector.xAu) && std::isfinite(vector.yAu) && std::isfinite(vector.zAu);
}

[[nodiscard]] CelestialFrameVector celestialVectorFromSolarSystemVector(const SolarSystemKernelVector& vector) noexcept
{
    return {
        .x = vector.xAu,
        .y = vector.yAu,
        .z = vector.zAu,
    };
}

[[nodiscard]] std::optional<CelestialFrameVector> observerItrsPositionAu(const core::GeoLocation& observer) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }

    const double latitudeRad = core::AngleMath::toRadians(observer.latitudeDeg);
    const double longitudeRad = core::AngleMath::toRadians(observer.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);
    const double firstEccentricitySquared = kWgs84Flattening * (2.0 - kWgs84Flattening);
    const double primeVerticalRadius =
        kWgs84EquatorialRadiusMeters / std::sqrt(1.0 - firstEccentricitySquared * sinLatitude * sinLatitude);

    const double xMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * cosLongitude;
    const double yMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * sinLongitude;
    const double zMeters =
        (primeVerticalRadius * (1.0 - firstEccentricitySquared) + observer.elevationMeters) * sinLatitude;
    return CelestialFrameVector{
        .x = xMeters / kAstronomicalUnitMeters,
        .y = yMeters / kAstronomicalUnitMeters,
        .z = zMeters / kAstronomicalUnitMeters,
    };
}

[[nodiscard]] CelestialFrameVector
subtractVector(const CelestialFrameVector& lhs, const CelestialFrameVector& rhs) noexcept
{
    return {
        .x = lhs.x - rhs.x,
        .y = lhs.y - rhs.y,
        .z = lhs.z - rhs.z,
    };
}

[[nodiscard]] std::optional<core::EquatorialCoordinate> equatorialFromVector(const CelestialFrameVector& vector
) noexcept
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

[[nodiscard]] std::optional<core::HorizontalCoordinate>
horizontalFromItrsVector(const CelestialFrameVector& vector, const core::GeoLocation& observer) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }
    if (!std::isfinite(vector.x) || !std::isfinite(vector.y) || !std::isfinite(vector.z)) {
        return std::nullopt;
    }

    const double latitudeRad = core::AngleMath::toRadians(observer.latitudeDeg);
    const double longitudeRad = core::AngleMath::toRadians(observer.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);

    const double east = -sinLongitude * vector.x + cosLongitude * vector.y;
    const double north =
        -sinLatitude * cosLongitude * vector.x - sinLatitude * sinLongitude * vector.y + cosLatitude * vector.z;
    const double up =
        cosLatitude * cosLongitude * vector.x + cosLatitude * sinLongitude * vector.y + sinLatitude * vector.z;
    const double length = std::hypot(std::hypot(east, north), up);
    if (length <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    return core::HorizontalCoordinate{
        .altitudeDeg = core::AngleMath::toDegrees(std::asin(std::clamp(up / length, -1.0, 1.0))),
        .azimuthDeg = core::AngleMath::normalizeDegrees(core::AngleMath::toDegrees(std::atan2(east, north))),
    };
}

void mergeMetadata(EphemerisResultMetadata& target, const EphemerisResultMetadata& source) noexcept
{
    if (source.status == EphemerisResultStatus::Failed) {
        target.status = EphemerisResultStatus::Failed;
    } else if (source.status == EphemerisResultStatus::Degraded && target.status == EphemerisResultStatus::Valid) {
        target.status = EphemerisResultStatus::Degraded;
    }

    target.warningCodeMask |= source.warningCodeMask;
    target.appliedCorrections |= source.appliedCorrections;
    if (target.dataSourceProvenance.empty()) {
        target.dataSourceProvenance = source.dataSourceProvenance;
    }
}

void markCorrectionUnavailable(EphemerisResultMetadata& metadata) noexcept
{
    if (metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
    }
    metadata.addWarning(EphemerisWarningCode::CorrectionUnavailable);
}

void mergeTimeScaleMetadata(EphemerisResultMetadata& metadata, const TimeScaleConversionResult& conversion) noexcept
{
    if (conversion.status == TimeScaleConversionStatus::Failed) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return;
    }
    if (conversion.status == TimeScaleConversionStatus::Degraded && metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    }
}

void mergeEarthOrientationMetadata(EphemerisResultMetadata& metadata, const EarthOrientationSample& sample) noexcept
{
    if (sample.status == EarthOrientationSampleStatus::Failed) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return;
    }
    if (sample.status == EarthOrientationSampleStatus::Degraded && metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::MissingData)) {
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::EpochOutsideRange)) {
        metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    }
}

[[nodiscard]] CelestialFrameVector
computationVectorFromCalculatorResult(const HighPrecisionCalculatorResult& calculatorResult) noexcept
{
    if (calculatorResult.observerRelativePositionAu.has_value()
        && isFiniteSolarSystemVector(*calculatorResult.observerRelativePositionAu)) {
        return celestialVectorFromSolarSystemVector(*calculatorResult.observerRelativePositionAu);
    }

    return vectorFromEquatorial(*calculatorResult.equatorial);
}

}  // namespace

ApparentPlaceCalculator::ApparentPlaceCalculator(
    std::shared_ptr<const IFrameTransformer> frameTransformer,
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider,
    std::shared_ptr<const IAtmosphericRefractionCalculator> atmosphericRefractionCalculator
)
    : m_frameTransformer(std::move(frameTransformer)), m_timeScaleService(std::move(timeScaleService)),
      m_earthOrientationProvider(std::move(earthOrientationProvider)),
      m_atmosphericRefractionCalculator(std::move(atmosphericRefractionCalculator))
{
}

HighPrecisionCalculatorResult ApparentPlaceCalculator::apply(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    HighPrecisionCalculatorResult result = calculatorResult;
    if (!calculatorResult.equatorial.has_value() || !isFiniteEquatorial(*calculatorResult.equatorial)) {
        return result;
    }
    if (m_frameTransformer == nullptr) {
        markCorrectionUnavailable(result.metadata);
        return result;
    }

    const EphemerisCorrectionFlags requestedCorrections = input.request.options.correctionFlags;
    const ApparentPlaceRequestMode requestMode = requestModeForCorrections(requestedCorrections);
    const CelestialReferenceFrame targetFrame = targetFrameForRequest(requestMode);
    if (targetFrame == CelestialReferenceFrame::Itrs) {
        if (m_timeScaleService == nullptr) {
            result.metadata.status = EphemerisResultStatus::Failed;
            result.metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            return result;
        }

        const TimeScaleConversionResult utcConversion =
            m_timeScaleService->convert(input.request.epoch, TimeScale::Utc);
        mergeTimeScaleMetadata(result.metadata, utcConversion);
        if (!utcConversion.isSuccess()) {
            markCorrectionUnavailable(result.metadata);
            return result;
        }

        const EarthOrientationSample earthOrientationSample = sampleEarthOrientation(
            m_earthOrientationProvider,
            utcConversion.epoch,
            EarthOrientationSampleOptions{
                .allowOutOfRangeNearestSampleFallback = true,
                .allowMissingDataZeroFallback = true,
            }
        );
        mergeEarthOrientationMetadata(result.metadata, earthOrientationSample);
        if (!earthOrientationSample.isSuccess()) {
            markCorrectionUnavailable(result.metadata);
            return result;
        }
    }

    CelestialFrameTransformResult transformResult = m_frameTransformer->transformCelestialVector({
        .sourceFrame = CelestialReferenceFrame::Gcrs,
        .targetFrame = targetFrame,
        .epoch = input.request.epoch,
        .vector = computationVectorFromCalculatorResult(calculatorResult),
    });
    mergeMetadata(result.metadata, transformResult.metadata);

    if (!transformResult.vector.has_value()) {
        markCorrectionUnavailable(result.metadata);
        return result;
    }

    CelestialFrameVector outputVector = *transformResult.vector;
    if (targetFrame == CelestialReferenceFrame::Itrs) {
        const std::optional<CelestialFrameVector> observerPosition =
            observerItrsPositionAu(input.request.context.observer);
        if (!observerPosition.has_value()) {
            if (result.metadata.status == EphemerisResultStatus::Valid) {
                result.metadata.status = EphemerisResultStatus::Degraded;
            }
            result.metadata.addWarning(EphemerisWarningCode::MissingObserver);
        } else if (calculatorResult.observerRelativePositionAu.has_value()) {
            outputVector = subtractVector(outputVector, *observerPosition);
            result.metadata.appliedCorrections |= EphemerisCorrectionFlags::DiurnalParallax;
        } else {
            markCorrectionUnavailable(result.metadata);
        }
    }

    std::optional<CelestialFrameVector> equatorialVector = outputVector;
    if (targetFrame == CelestialReferenceFrame::Itrs) {
        CelestialFrameTransformResult gcrsTransformResult = m_frameTransformer->transformCelestialVector({
            .sourceFrame = CelestialReferenceFrame::Itrs,
            .targetFrame = CelestialReferenceFrame::Gcrs,
            .epoch = input.request.epoch,
            .vector = outputVector,
        });
        mergeMetadata(result.metadata, gcrsTransformResult.metadata);
        if (gcrsTransformResult.vector.has_value()) {
            equatorialVector = *gcrsTransformResult.vector;
        } else {
            markCorrectionUnavailable(result.metadata);
        }

        if (equatorialVector.has_value()
            && hasCorrectionFlag(requestedCorrections, EphemerisCorrectionFlags::PrecessionNutation)) {
            CelestialFrameTransformResult apparentEquatorialTransformResult =
                m_frameTransformer->transformCelestialVector({
                    .sourceFrame = CelestialReferenceFrame::Gcrs,
                    .targetFrame = CelestialReferenceFrame::TrueEquatorAndEquinox,
                    .epoch = input.request.epoch,
                    .vector = *equatorialVector,
                });
            mergeMetadata(result.metadata, apparentEquatorialTransformResult.metadata);
            if (apparentEquatorialTransformResult.vector.has_value()) {
                equatorialVector = *apparentEquatorialTransformResult.vector;
            } else {
                markCorrectionUnavailable(result.metadata);
            }
        }
    }

    if (const std::optional<core::EquatorialCoordinate> equatorial = equatorialFromVector(*equatorialVector);
        equatorial.has_value()) {
        result.equatorial = *equatorial;
    } else {
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return result;
    }

    if (targetFrame == CelestialReferenceFrame::Itrs) {
        if (const std::optional<core::HorizontalCoordinate> horizontal =
                horizontalFromItrsVector(outputVector, input.request.context.observer);
            horizontal.has_value()) {
            result.horizontal = *horizontal;
        } else {
            if (result.metadata.status == EphemerisResultStatus::Valid) {
                result.metadata.status = EphemerisResultStatus::Degraded;
            }
            result.metadata.addWarning(EphemerisWarningCode::MissingObserver);
        }
    }

    if (input.request.options.enableAtmosphericRefraction
        && hasCorrectionFlag(requestedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction)) {
        if (m_atmosphericRefractionCalculator == nullptr) {
            markCorrectionUnavailable(result.metadata);
        } else {
            result = m_atmosphericRefractionCalculator->apply(input, result);
        }
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
