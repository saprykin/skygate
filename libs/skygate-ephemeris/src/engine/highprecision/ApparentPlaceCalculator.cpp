#include "engine/highprecision/ApparentPlaceCalculator.hpp"

#include "engine/highprecision/FrameTransformer.hpp"
#include "skygate/core/math/AngleMath.hpp"
#include "skygate/ephemeris/EarthOrientationProvider.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr double kRadiansPerHour = 3.141592653589793238462643383279502884 / 12.0;
constexpr double kHoursPerRadian = 12.0 / 3.141592653589793238462643383279502884;

[[nodiscard]] bool isFiniteEquatorial(const core::EquatorialCoordinate& coordinate) noexcept
{
    return std::isfinite(coordinate.rightAscensionHours) && std::isfinite(coordinate.declinationDeg);
}

[[nodiscard]] bool requestsTopocentricOutput(const EphemerisCorrectionFlags flags) noexcept
{
    return hasCorrectionFlag(flags, EphemerisCorrectionFlags::DiurnalParallax)
           || hasCorrectionFlag(flags, EphemerisCorrectionFlags::EarthOrientation);
}

[[nodiscard]] CelestialReferenceFrame targetFrameForRequest(const EphemerisCorrectionFlags flags) noexcept
{
    return requestsTopocentricOutput(flags) ? CelestialReferenceFrame::Itrs : CelestialReferenceFrame::Cirs;
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
}

}  // namespace

ApparentPlaceCalculator::ApparentPlaceCalculator(
    std::shared_ptr<const IFrameTransformer> frameTransformer,
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider
)
    : m_frameTransformer(std::move(frameTransformer)), m_timeScaleService(std::move(timeScaleService)),
      m_earthOrientationProvider(std::move(earthOrientationProvider))
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
    const CelestialReferenceFrame targetFrame = targetFrameForRequest(requestedCorrections);
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
        .vector = vectorFromEquatorial(*calculatorResult.equatorial),
    });
    mergeMetadata(result.metadata, transformResult.metadata);

    if (!transformResult.vector.has_value()) {
        markCorrectionUnavailable(result.metadata);
        return result;
    }

    if (const std::optional<core::EquatorialCoordinate> equatorial = equatorialFromVector(*transformResult.vector);
        equatorial.has_value()) {
        result.equatorial = *equatorial;
    } else {
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        return result;
    }

    if (targetFrame == CelestialReferenceFrame::Itrs) {
        if (const std::optional<core::HorizontalCoordinate> horizontal =
                horizontalFromItrsVector(*transformResult.vector, input.request.context.observer);
            horizontal.has_value()) {
            result.horizontal = *horizontal;
            result.metadata.appliedCorrections |= EphemerisCorrectionFlags::DiurnalParallax;
        } else {
            if (result.metadata.status == EphemerisResultStatus::Valid) {
                result.metadata.status = EphemerisResultStatus::Degraded;
            }
            result.metadata.addWarning(EphemerisWarningCode::MissingObserver);
        }
    }

    if (input.request.options.enableAtmosphericRefraction
        && hasCorrectionFlag(requestedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction)) {
        markCorrectionUnavailable(result.metadata);
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
