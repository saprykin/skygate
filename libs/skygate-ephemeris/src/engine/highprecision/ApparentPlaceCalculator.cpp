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

[[nodiscard]] std::optional<CelestialFrameVector>
celestialVectorFromSolarSystemVector(const std::optional<SolarSystemKernelVector>& vector) noexcept
{
    if (!vector.has_value()) {
        return std::nullopt;
    }
    return celestialVectorFromSolarSystemVector(*vector);
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

[[nodiscard]] std::optional<core::EquatorialCoordinate>
equatorialFromVector(const CelestialFrameVector& vector) noexcept
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
    target.unavailableCorrections |= source.unavailableCorrections;
    if (target.dataSourceProvenance.empty()) {
        target.dataSourceProvenance = source.dataSourceProvenance;
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

void markMissingBatchTransformResult(
    EphemerisResultMetadata& metadata, const EphemerisCorrectionFlags unavailableCorrection
) noexcept
{
    if (metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
    }
    metadata.addWarning(EphemerisWarningCode::ComputationFailed);
    metadata.addUnavailableCorrection(unavailableCorrection);
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
    const EphemerisCorrectionFlags requestedCorrections = input.request.options.correctionFlags;
    const ApparentPlaceRequestMode requestMode = requestModeForCorrections(requestedCorrections);
    const CelestialReferenceFrame targetFrame = targetFrameForRequest(requestMode);
    if (targetFrame == CelestialReferenceFrame::Itrs) {
        if (input.preparedRequestState != nullptr && input.preparedRequestState->topocentricStatePrepared) {
            mergeMetadata(result.metadata, input.preparedRequestState->topocentricMetadata);
            if (!input.preparedRequestState->topocentricStateAvailable) {
                return result;
            }
        } else if (m_timeScaleService == nullptr) {
            result.metadata.status = EphemerisResultStatus::Failed;
            result.metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::EarthOrientation);
            return result;
        }

        const TimeScaleConversionResult utcConversion =
            m_timeScaleService->convert(input.request.epoch, TimeScale::Utc);
        mergeTimeScaleMetadata(result.metadata, utcConversion);
        if (!utcConversion.isSuccess()) {
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::EarthOrientation);
            return result;
        }

        const EarthOrientationSample earthOrientationSample = sampleEarthOrientation(
            m_earthOrientationProvider,
            utcConversion.epoch,
            EarthOrientationSampleOptions{
                .allowOutOfRangeNearestSampleFallback = true,
                .allowMissingDataZeroFallback = true,
                .degradePredictedData = false,
            }
        );
        mergeEarthOrientationMetadata(result.metadata, earthOrientationSample);
        if (!earthOrientationSample.isSuccess()) {
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::EarthOrientation);
            return result;
        }
    }

    CelestialFrameVector outputVector = computationVectorFromCalculatorResult(calculatorResult);
    if (m_frameTransformer != nullptr) {
        CelestialFrameTransformResult transformResult = m_frameTransformer->transformCelestialVector({
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = targetFrame,
            .epoch = input.request.epoch,
            .vector = outputVector,
        });
        mergeMetadata(result.metadata, transformResult.metadata);

        if (!transformResult.vector.has_value()) {
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::PrecessionNutation);
            return result;
        }
        outputVector = *transformResult.vector;
    } else if (targetFrame != CelestialReferenceFrame::Gcrs) {
        markCorrectionUnavailable(
            result.metadata,
            targetFrame == CelestialReferenceFrame::Itrs
                ? (EphemerisCorrectionFlags::PrecessionNutation | EphemerisCorrectionFlags::EarthOrientation)
                : EphemerisCorrectionFlags::PrecessionNutation
        );
        return result;
    }
    if (targetFrame == CelestialReferenceFrame::Itrs) {
        const std::optional<CelestialFrameVector> observerPosition =
            input.preparedRequestState != nullptr && input.preparedRequestState->topocentricStatePrepared
                ? celestialVectorFromSolarSystemVector(input.preparedRequestState->observerItrsPositionAu)
                : observerItrsPositionAu(input.request.context.observer);
        if (!observerPosition.has_value()) {
            if (result.metadata.status == EphemerisResultStatus::Valid) {
                result.metadata.status = EphemerisResultStatus::Degraded;
            }
            result.metadata.addWarning(EphemerisWarningCode::MissingObserver);
            result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::DiurnalParallax);
        } else if (calculatorResult.observerRelativePositionAu.has_value()) {
            outputVector = subtractVector(outputVector, *observerPosition);
            result.metadata.appliedCorrections |= EphemerisCorrectionFlags::DiurnalParallax;
        } else {
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::DiurnalParallax);
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
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::EarthOrientation);
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
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::PrecessionNutation);
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
            markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::AtmosphericRefraction);
        } else {
            result = m_atmosphericRefractionCalculator->apply(input, result);
        }
    }

    return result;
}

std::vector<StarAstrometryBatchResult> ApparentPlaceCalculator::applyBatch(
    const EphemerisRequest& request,
    const std::span<const CelestialBody> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    const EphemerisCorrectionFlags requestedCorrections = request.options.correctionFlags;
    const ApparentPlaceRequestMode requestMode = requestModeForCorrections(requestedCorrections);
    const CelestialReferenceFrame targetFrame = targetFrameForRequest(requestMode);
    const bool isTopocentric = targetFrame == CelestialReferenceFrame::Itrs;
    const bool requestsAtmosphericRefraction =
        request.options.enableAtmosphericRefraction
        && hasCorrectionFlag(requestedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction);

    EphemerisResultMetadata topocentricMetadata;
    bool topocentricRequestWideStateAvailable = true;
    if (isTopocentric) {
        if (preparedRequestState != nullptr && preparedRequestState->topocentricStatePrepared) {
            topocentricMetadata = preparedRequestState->topocentricMetadata;
            topocentricRequestWideStateAvailable = preparedRequestState->topocentricStateAvailable;
        } else if (m_timeScaleService == nullptr) {
            topocentricMetadata.status = EphemerisResultStatus::Failed;
            topocentricMetadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
            topocentricMetadata.addUnavailableCorrection(EphemerisCorrectionFlags::EarthOrientation);
            topocentricRequestWideStateAvailable = false;
        } else {
            const TimeScaleConversionResult utcConversion = m_timeScaleService->convert(request.epoch, TimeScale::Utc);
            mergeTimeScaleMetadata(topocentricMetadata, utcConversion);
            if (!utcConversion.isSuccess()) {
                markCorrectionUnavailable(topocentricMetadata, EphemerisCorrectionFlags::EarthOrientation);
                topocentricRequestWideStateAvailable = false;
            } else {
                const EarthOrientationSample earthOrientationSample = sampleEarthOrientation(
                    m_earthOrientationProvider,
                    utcConversion.epoch,
                    EarthOrientationSampleOptions{
                        .allowOutOfRangeNearestSampleFallback = true,
                        .allowMissingDataZeroFallback = true,
                        .degradePredictedData = false,
                    }
                );
                mergeEarthOrientationMetadata(topocentricMetadata, earthOrientationSample);
                if (!earthOrientationSample.isSuccess()) {
                    markCorrectionUnavailable(topocentricMetadata, EphemerisCorrectionFlags::EarthOrientation);
                    topocentricRequestWideStateAvailable = false;
                }
            }
        }
    }

    std::vector<StarAstrometryBatchResult> results;
    results.reserve(calculatorResults.size());
    std::vector<std::optional<CelestialFrameVector>> outputVectors;
    outputVectors.reserve(calculatorResults.size());
    std::vector<bool> hasObserverRelativePosition;
    hasObserverRelativePosition.reserve(calculatorResults.size());
    std::vector<CelestialFrameVector> transformInputs;
    transformInputs.reserve(calculatorResults.size());
    std::vector<std::size_t> transformResultIndices;
    transformResultIndices.reserve(calculatorResults.size());

    for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
        if (calculatorResult.bodyIndex >= bodies.size()) {
            continue;
        }

        HighPrecisionCalculatorResult result = calculatorResult.result;
        if (!calculatorResult.result.equatorial.has_value()
            || !isFiniteEquatorial(*calculatorResult.result.equatorial)) {
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = std::move(result),
                }
            );
            outputVectors.push_back(std::nullopt);
            hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
            continue;
        }
        if (isTopocentric) {
            mergeMetadata(result.metadata, topocentricMetadata);
            if (!topocentricRequestWideStateAvailable) {
                results.push_back(
                    StarAstrometryBatchResult{
                        .bodyIndex = calculatorResult.bodyIndex,
                        .result = std::move(result),
                    }
                );
                outputVectors.push_back(std::nullopt);
                hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
                continue;
            }
        }

        CelestialFrameVector outputVector = computationVectorFromCalculatorResult(calculatorResult.result);
        if (targetFrame == CelestialReferenceFrame::Gcrs) {
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = std::move(result),
                }
            );
            outputVectors.push_back(outputVector);
            hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
            continue;
        }

        results.push_back(
            StarAstrometryBatchResult{
                .bodyIndex = calculatorResult.bodyIndex,
                .result = std::move(result),
            }
        );
        outputVectors.push_back(std::nullopt);
        hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
        if (m_frameTransformer != nullptr) {
            transformInputs.push_back(outputVector);
            transformResultIndices.push_back(results.size() - 1U);
        } else {
            markCorrectionUnavailable(
                results.back().result.metadata,
                isTopocentric
                    ? (EphemerisCorrectionFlags::PrecessionNutation | EphemerisCorrectionFlags::EarthOrientation)
                    : EphemerisCorrectionFlags::PrecessionNutation
            );
        }
    }

    if (m_frameTransformer != nullptr && !transformInputs.empty()) {
        const std::vector<CelestialFrameTransformResult> transformResults =
            m_frameTransformer->transformCelestialVectors(
                CelestialFrameBatchTransformRequest{
                    .sourceFrame = CelestialReferenceFrame::Gcrs,
                    .targetFrame = targetFrame,
                    .epoch = request.epoch,
                    .vectors = transformInputs,
                }
            );
        const std::size_t transformCount = std::min(transformResults.size(), transformResultIndices.size());
        for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
            const std::size_t resultIndex = transformResultIndices[transformIndex];
            HighPrecisionCalculatorResult& result = results[resultIndex].result;
            const CelestialFrameTransformResult& transformResult = transformResults[transformIndex];
            mergeMetadata(result.metadata, transformResult.metadata);
            if (!transformResult.vector.has_value()) {
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::PrecessionNutation);
                continue;
            }

            outputVectors[resultIndex] = *transformResult.vector;
        }
        const EphemerisCorrectionFlags unavailableCorrection =
            isTopocentric ? (EphemerisCorrectionFlags::PrecessionNutation | EphemerisCorrectionFlags::EarthOrientation)
                          : EphemerisCorrectionFlags::PrecessionNutation;
        for (std::size_t transformIndex = transformCount; transformIndex < transformResultIndices.size();
             ++transformIndex) {
            HighPrecisionCalculatorResult& result = results[transformResultIndices[transformIndex]].result;
            markMissingBatchTransformResult(result.metadata, unavailableCorrection);
        }
    }

    std::vector<std::optional<CelestialFrameVector>> equatorialVectors = outputVectors;
    if (isTopocentric) {
        const std::optional<CelestialFrameVector> observerPosition =
            preparedRequestState != nullptr && preparedRequestState->topocentricStatePrepared
                ? celestialVectorFromSolarSystemVector(preparedRequestState->observerItrsPositionAu)
                : observerItrsPositionAu(request.context.observer);
        for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
            if (!outputVectors[resultIndex].has_value()) {
                continue;
            }

            HighPrecisionCalculatorResult& result = results[resultIndex].result;
            if (!observerPosition.has_value()) {
                if (result.metadata.status == EphemerisResultStatus::Valid) {
                    result.metadata.status = EphemerisResultStatus::Degraded;
                }
                result.metadata.addWarning(EphemerisWarningCode::MissingObserver);
                result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::DiurnalParallax);
            } else if (hasObserverRelativePosition[resultIndex]) {
                outputVectors[resultIndex] = subtractVector(*outputVectors[resultIndex], *observerPosition);
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::DiurnalParallax;
            } else {
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::DiurnalParallax);
            }

            equatorialVectors[resultIndex] = outputVectors[resultIndex];
            if (const std::optional<core::HorizontalCoordinate> horizontal =
                    horizontalFromItrsVector(*outputVectors[resultIndex], request.context.observer);
                horizontal.has_value()) {
                result.horizontal = *horizontal;
            } else {
                if (result.metadata.status == EphemerisResultStatus::Valid) {
                    result.metadata.status = EphemerisResultStatus::Degraded;
                }
                result.metadata.addWarning(EphemerisWarningCode::MissingObserver);
            }
        }

        std::vector<CelestialFrameVector> gcrsInputs;
        std::vector<std::size_t> gcrsResultIndices;
        gcrsInputs.reserve(results.size());
        gcrsResultIndices.reserve(results.size());
        for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
            if (outputVectors[resultIndex].has_value()) {
                gcrsInputs.push_back(*outputVectors[resultIndex]);
                gcrsResultIndices.push_back(resultIndex);
            }
        }

        if (m_frameTransformer != nullptr && !gcrsInputs.empty()) {
            const std::vector<CelestialFrameTransformResult> gcrsTransformResults =
                m_frameTransformer->transformCelestialVectors(
                    CelestialFrameBatchTransformRequest{
                        .sourceFrame = CelestialReferenceFrame::Itrs,
                        .targetFrame = CelestialReferenceFrame::Gcrs,
                        .epoch = request.epoch,
                        .vectors = gcrsInputs,
                    }
                );
            const std::size_t transformCount = std::min(gcrsTransformResults.size(), gcrsResultIndices.size());
            for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
                const std::size_t resultIndex = gcrsResultIndices[transformIndex];
                HighPrecisionCalculatorResult& result = results[resultIndex].result;
                const CelestialFrameTransformResult& transformResult = gcrsTransformResults[transformIndex];
                mergeMetadata(result.metadata, transformResult.metadata);
                if (transformResult.vector.has_value()) {
                    equatorialVectors[resultIndex] = *transformResult.vector;
                } else {
                    markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::EarthOrientation);
                }
            }
            for (std::size_t transformIndex = transformCount; transformIndex < gcrsResultIndices.size();
                 ++transformIndex) {
                HighPrecisionCalculatorResult& result = results[gcrsResultIndices[transformIndex]].result;
                markMissingBatchTransformResult(result.metadata, EphemerisCorrectionFlags::EarthOrientation);
            }
        }

        if (hasCorrectionFlag(requestedCorrections, EphemerisCorrectionFlags::PrecessionNutation)) {
            std::vector<CelestialFrameVector> apparentInputs;
            std::vector<std::size_t> apparentResultIndices;
            apparentInputs.reserve(results.size());
            apparentResultIndices.reserve(results.size());
            for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
                if (equatorialVectors[resultIndex].has_value()) {
                    apparentInputs.push_back(*equatorialVectors[resultIndex]);
                    apparentResultIndices.push_back(resultIndex);
                }
            }

            if (m_frameTransformer != nullptr && !apparentInputs.empty()) {
                const std::vector<CelestialFrameTransformResult> apparentTransformResults =
                    m_frameTransformer->transformCelestialVectors(
                        CelestialFrameBatchTransformRequest{
                            .sourceFrame = CelestialReferenceFrame::Gcrs,
                            .targetFrame = CelestialReferenceFrame::TrueEquatorAndEquinox,
                            .epoch = request.epoch,
                            .vectors = apparentInputs,
                        }
                    );
                const std::size_t transformCount =
                    std::min(apparentTransformResults.size(), apparentResultIndices.size());
                for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
                    const std::size_t resultIndex = apparentResultIndices[transformIndex];
                    HighPrecisionCalculatorResult& result = results[resultIndex].result;
                    const CelestialFrameTransformResult& transformResult = apparentTransformResults[transformIndex];
                    mergeMetadata(result.metadata, transformResult.metadata);
                    if (transformResult.vector.has_value()) {
                        equatorialVectors[resultIndex] = *transformResult.vector;
                    } else {
                        markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::PrecessionNutation);
                    }
                }
                for (std::size_t transformIndex = transformCount; transformIndex < apparentResultIndices.size();
                     ++transformIndex) {
                    HighPrecisionCalculatorResult& result = results[apparentResultIndices[transformIndex]].result;
                    markMissingBatchTransformResult(result.metadata, EphemerisCorrectionFlags::PrecessionNutation);
                }
            }
        }
    }

    for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
        HighPrecisionCalculatorResult& result = results[resultIndex].result;
        if (!equatorialVectors[resultIndex].has_value()) {
            continue;
        }

        if (const std::optional<core::EquatorialCoordinate> equatorial =
                equatorialFromVector(*equatorialVectors[resultIndex]);
            equatorial.has_value()) {
            result.equatorial = *equatorial;
        } else {
            result.metadata.status = EphemerisResultStatus::Failed;
            result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        }

        if (requestsAtmosphericRefraction) {
            if (m_atmosphericRefractionCalculator == nullptr) {
                markCorrectionUnavailable(result.metadata, EphemerisCorrectionFlags::AtmosphericRefraction);
            } else {
                const HighPrecisionComputationInput input{
                    .request = request,
                    .body = bodies[results[resultIndex].bodyIndex],
                    .preparedRequestState = preparedRequestState,
                    .bodyIndex = results[resultIndex].bodyIndex,
                };
                result = m_atmosphericRefractionCalculator->apply(input, result);
            }
        }
    }

    return results;
}

}  // namespace skygate::ephemeris::highprecision
