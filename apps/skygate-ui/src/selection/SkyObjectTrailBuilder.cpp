#include "SkyObjectTrailBuilder.hpp"

#include "SkyPerformanceLogging.hpp"
#include "SkyRenderLabels.hpp"

#include "math/Geometry2d.hpp"
#include "math/LinePattern.hpp"
#include "math/ProjectedPolylineBuilder.hpp"
#include "CelestialReferenceCalculator.hpp"
#include "EphemerisEngineFactory.hpp"
#include "IEphemerisEngine.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QString>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

namespace {

constexpr int kObjectTrailPastHours = 6;
constexpr int kObjectTrailFutureHours = 18;
constexpr int kObjectTrailSampleStepMinutes = 30;
constexpr int kObjectTrailHighPrecisionRenderStepMinutes = 10;
constexpr int kObjectTrailHighPrecisionSampleStepMinutes = 120;
constexpr double kObjectTrailHighPrecisionMaxInterpolationErrorDeg = 0.05;
constexpr double kObjectTrailPastWidthPx = 1.4;
constexpr double kObjectTrailFutureWidthPx = 2.0;
constexpr double kObjectTrailPastDashLengthPx = 8.0;
constexpr double kObjectTrailPastDashGapPx = 7.0;
constexpr int kObjectTrailFutureTickStepHours = 6;
constexpr double kObjectTrailFutureTickRadiusPx = 5.0;

QColor colorWithAlpha(const QColor& color, const int alpha)
{
    QColor adjusted = color;
    adjusted.setAlpha(alpha);
    return adjusted;
}

[[nodiscard]] bool usesHighPrecisionRequest(const SkyObjectTrailInput& input) noexcept
{
    return input.ephemerisRequest.has_value()
           && input.ephemerisRequest->options.engineKind == skygate::ephemeris::EphemerisEngineKind::HighPrecision;
}

[[nodiscard]] bool isFixedEquatorialTrailTarget(const SkyObjectTrailInput& input) noexcept
{
    if (!usesHighPrecisionRequest(input) || input.targetBody == nullptr || input.targetState == nullptr
        || !input.targetState->equatorial.isFinite()) {
        return false;
    }

    return input.targetBody->fixedEquatorial.has_value() || input.targetBody->starAstrometry.has_value()
           || input.targetBody->ephemerisSource == skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
           || input.targetBody->ephemerisSource == skygate::ephemeris::CelestialBodyEphemerisSource::Star;
}

[[nodiscard]] bool shouldUseGuidanceTrail(const SkyObjectTrailInput& input) noexcept
{
    return usesHighPrecisionRequest(input) && input.targetBody != nullptr && !isFixedEquatorialTrailTarget(input);
}

[[nodiscard]] bool
epochsEqual(const skygate::ephemeris::AstronomicalEpoch& lhs, const skygate::ephemeris::AstronomicalEpoch& rhs) noexcept
{
    return lhs.julianDatePart1 == rhs.julianDatePart1 && lhs.julianDatePart2 == rhs.julianDatePart2
           && lhs.timeScale == rhs.timeScale;
}

[[nodiscard]] bool optionalEpochsEqual(
    const std::optional<skygate::ephemeris::AstronomicalEpoch>& lhs,
    const std::optional<skygate::ephemeris::AstronomicalEpoch>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || epochsEqual(*lhs, *rhs);
}

[[nodiscard]] bool optionsEqual(
    const skygate::ephemeris::EphemerisEngineOptions& lhs, const skygate::ephemeris::EphemerisEngineOptions& rhs
) noexcept
{
    return lhs.engineKind == rhs.engineKind && lhs.correctionFlags == rhs.correctionFlags
           && lhs.fallbackToSimpleEngine == rhs.fallbackToSimpleEngine
           && lhs.enableAtmosphericRefraction == rhs.enableAtmosphericRefraction
           && lhs.atmosphericPressureHpa == rhs.atmosphericPressureHpa
           && lhs.atmosphericTemperatureC == rhs.atmosphericTemperatureC && lhs.relativeHumidity == rhs.relativeHumidity
           && lhs.observingWavelengthMicrometers == rhs.observingWavelengthMicrometers;
}

[[nodiscard]] bool optionalOptionsEqual(
    const std::optional<skygate::ephemeris::EphemerisEngineOptions>& lhs,
    const std::optional<skygate::ephemeris::EphemerisEngineOptions>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || optionsEqual(*lhs, *rhs);
}

[[nodiscard]] bool
equatorialEqual(const skygate::core::EquatorialCoordinate& lhs, const skygate::core::EquatorialCoordinate& rhs) noexcept
{
    return lhs.rightAscensionHours == rhs.rightAscensionHours && lhs.declinationDeg == rhs.declinationDeg;
}

[[nodiscard]] bool optionalEquatorialsEqual(
    const std::optional<skygate::core::EquatorialCoordinate>& lhs,
    const std::optional<skygate::core::EquatorialCoordinate>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || equatorialEqual(*lhs, *rhs);
}

[[nodiscard]] bool observersEqual(const skygate::core::GeoLocation& lhs, const skygate::core::GeoLocation& rhs) noexcept
{
    return lhs.latitudeDeg == rhs.latitudeDeg && lhs.longitudeDeg == rhs.longitudeDeg
           && lhs.elevationMeters == rhs.elevationMeters;
}

[[nodiscard]] double normalizeDegrees(const double valueDeg) noexcept
{
    double normalizedDeg = std::fmod(valueDeg, 360.0);
    return normalizedDeg < 0.0 ? normalizedDeg + 360.0 : normalizedDeg;
}

[[nodiscard]] skygate::ephemeris::AstronomicalEpoch
addMinutes(const skygate::ephemeris::AstronomicalEpoch& epoch, const int offsetMinutes) noexcept
{
    return skygate::ephemeris::normalizedAstronomicalEpoch(
        skygate::ephemeris::AstronomicalEpoch{
            .julianDatePart1 = epoch.julianDatePart1,
            .julianDatePart2 = epoch.julianDatePart2 + static_cast<double>(offsetMinutes) / (24.0 * 60.0),
            .timeScale = epoch.timeScale
        }
    );
}

struct UnitVector3d final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] UnitVector3d horizontalToUnitVector(const skygate::core::HorizontalCoordinate& coordinate) noexcept
{
    constexpr double degreesToRadians = std::numbers::pi / 180.0;
    const double altitudeRad = coordinate.altitudeDeg * degreesToRadians;
    const double azimuthRad = coordinate.azimuthDeg * degreesToRadians;
    const double cosAltitude = std::cos(altitudeRad);
    return UnitVector3d{
        .x = cosAltitude * std::sin(azimuthRad),
        .y = cosAltitude * std::cos(azimuthRad),
        .z = std::sin(altitudeRad),
    };
}

[[nodiscard]] skygate::core::HorizontalCoordinate unitVectorToHorizontal(const UnitVector3d& vector) noexcept
{
    constexpr double radiansToDegrees = 180.0 / std::numbers::pi;
    const double altitudeDeg = std::asin(std::clamp(vector.z, -1.0, 1.0)) * radiansToDegrees;
    const double azimuthDeg = normalizeDegrees(std::atan2(vector.x, vector.y) * radiansToDegrees);
    return skygate::core::HorizontalCoordinate{.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg};
}

[[nodiscard]] UnitVector3d normalized(const UnitVector3d& vector) noexcept
{
    const double length = std::sqrt((vector.x * vector.x) + (vector.y * vector.y) + (vector.z * vector.z));
    if (length <= 0.0) {
        return UnitVector3d{.x = 0.0, .y = 0.0, .z = 1.0};
    }

    return UnitVector3d{.x = vector.x / length, .y = vector.y / length, .z = vector.z / length};
}

[[nodiscard]] double dotProduct(const UnitVector3d& lhs, const UnitVector3d& rhs) noexcept
{
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] UnitVector3d crossProduct(const UnitVector3d& lhs, const UnitVector3d& rhs) noexcept
{
    return UnitVector3d{
        .x = (lhs.y * rhs.z) - (lhs.z * rhs.y),
        .y = (lhs.z * rhs.x) - (lhs.x * rhs.z),
        .z = (lhs.x * rhs.y) - (lhs.y * rhs.x),
    };
}

[[nodiscard]] double vectorLength(const UnitVector3d& vector) noexcept
{
    return std::sqrt((vector.x * vector.x) + (vector.y * vector.y) + (vector.z * vector.z));
}

[[nodiscard]] UnitVector3d perpendicularAxis(const UnitVector3d& vector) noexcept
{
    const UnitVector3d zAxis{.x = 0.0, .y = 0.0, .z = 1.0};
    UnitVector3d axis = crossProduct(vector, zAxis);
    if (vectorLength(axis) <= 1e-9) {
        axis = crossProduct(vector, UnitVector3d{.x = 1.0, .y = 0.0, .z = 0.0});
    }
    return normalized(axis);
}

[[nodiscard]] UnitVector3d
rotatedAroundAxis(const UnitVector3d& vector, const UnitVector3d& axis, const double angleRad) noexcept
{
    const double cosAngle = std::cos(angleRad);
    const double sinAngle = std::sin(angleRad);
    const UnitVector3d axisCrossVector = crossProduct(axis, vector);
    const double axisDotVector = dotProduct(axis, vector);
    return normalized(
        UnitVector3d{
            .x = (vector.x * cosAngle) + (axisCrossVector.x * sinAngle) + (axis.x * axisDotVector * (1.0 - cosAngle)),
            .y = (vector.y * cosAngle) + (axisCrossVector.y * sinAngle) + (axis.y * axisDotVector * (1.0 - cosAngle)),
            .z = (vector.z * cosAngle) + (axisCrossVector.z * sinAngle) + (axis.z * axisDotVector * (1.0 - cosAngle)),
        }
    );
}

[[nodiscard]] double angularSeparationDegrees(
    const skygate::core::HorizontalCoordinate& lhs, const skygate::core::HorizontalCoordinate& rhs
) noexcept
{
    constexpr double radiansToDegrees = 180.0 / std::numbers::pi;
    const UnitVector3d lhsVector = horizontalToUnitVector(lhs.normalizedAzimuth());
    const UnitVector3d rhsVector = horizontalToUnitVector(rhs.normalizedAzimuth());
    const double dot = std::clamp(dotProduct(lhsVector, rhsVector), -1.0, 1.0);
    return std::acos(dot) * radiansToDegrees;
}

[[nodiscard]] skygate::core::HorizontalCoordinate interpolateHorizontalCoordinate(
    const skygate::core::HorizontalCoordinate& previous,
    const skygate::core::HorizontalCoordinate& next,
    const double fraction
) noexcept
{
    const UnitVector3d previousVector = horizontalToUnitVector(previous);
    const UnitVector3d nextVector = horizontalToUnitVector(next);
    return unitVectorToHorizontal(normalized(
        UnitVector3d{
            .x = previousVector.x + ((nextVector.x - previousVector.x) * fraction),
            .y = previousVector.y + ((nextVector.y - previousVector.y) * fraction),
            .z = previousVector.z + ((nextVector.z - previousVector.z) * fraction),
        }
    ));
}

[[nodiscard]] std::optional<skygate::core::HorizontalCoordinate> interpolateSampleAtOffset(
    const std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const int offsetMinutes,
    std::size_t& nextAnchorIndex
)
{
    while (nextAnchorIndex < anchors.size() && anchors[nextAnchorIndex].offsetMinutes < offsetMinutes) {
        ++nextAnchorIndex;
    }
    if (nextAnchorIndex >= anchors.size()) {
        return std::nullopt;
    }

    const skygate::ephemeris::BodyTrailSample& next = anchors[nextAnchorIndex];
    if (next.offsetMinutes == offsetMinutes) {
        return next.horizontal;
    }
    if (nextAnchorIndex == 0U) {
        return std::nullopt;
    }

    const skygate::ephemeris::BodyTrailSample& previous = anchors[nextAnchorIndex - 1U];
    if (!previous.horizontal.has_value() || !next.horizontal.has_value()) {
        return std::nullopt;
    }

    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= 0) {
        return std::nullopt;
    }

    return interpolateHorizontalCoordinate(
        *previous.horizontal,
        *next.horizontal,
        static_cast<double>(offsetMinutes - previous.offsetMinutes) / static_cast<double>(spanMinutes)
    );
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample> interpolateTrailSamples(
    const std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    std::vector<skygate::ephemeris::BodyTrailSample> samples;
    const int startOffsetMinutes = -renderOptions.pastHours * 60;
    const int endOffsetMinutes = renderOptions.futureHours * 60;
    samples.reserve(
        static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / renderOptions.sampleStepMinutes) + 1U
    );

    std::size_t nextAnchorIndex = 0U;
    for (int offsetMinutes = startOffsetMinutes; offsetMinutes <= endOffsetMinutes;
         offsetMinutes += renderOptions.sampleStepMinutes) {
        samples.push_back(
            skygate::ephemeris::BodyTrailSample{
                .offsetMinutes = offsetMinutes,
                .horizontal = interpolateSampleAtOffset(anchors, offsetMinutes, nextAnchorIndex)
            }
        );
    }

    return samples;
}

void alignGuidanceTrailToSelectedState(
    std::vector<skygate::ephemeris::BodyTrailSample>& samples, const SkyObjectTrailInput& input
)
{
    if (input.targetState == nullptr || !input.targetState->horizontal.isFinite()) {
        return;
    }

    auto presentSample =
        std::find_if(samples.begin(), samples.end(), [](const skygate::ephemeris::BodyTrailSample& sample) {
            return sample.offsetMinutes == 0 && sample.horizontal.has_value() && sample.horizontal->isFinite();
        });
    if (presentSample == samples.end()) {
        return;
    }

    const skygate::core::HorizontalCoordinate selectedHorizontal = input.targetState->horizontal;
    const UnitVector3d guidanceNow = normalized(horizontalToUnitVector(*presentSample->horizontal));
    const UnitVector3d selectedNow = normalized(horizontalToUnitVector(selectedHorizontal));
    const double dot = std::clamp(dotProduct(guidanceNow, selectedNow), -1.0, 1.0);
    UnitVector3d axis = crossProduct(guidanceNow, selectedNow);
    const double axisLength = vectorLength(axis);
    double angleRad = 0.0;
    if (axisLength <= 1e-9) {
        if (dot > 0.0) {
            presentSample->horizontal = selectedHorizontal;
            return;
        }
        axis = perpendicularAxis(guidanceNow);
        angleRad = std::numbers::pi;
    } else {
        axis = normalized(axis);
        angleRad = std::atan2(axisLength, dot);
    }

    for (skygate::ephemeris::BodyTrailSample& sample : samples) {
        if (sample.horizontal.has_value() && sample.horizontal->isFinite()) {
            sample.horizontal =
                unitVectorToHorizontal(rotatedAroundAxis(horizontalToUnitVector(*sample.horizontal), axis, angleRad));
        }
    }
    presentSample->horizontal = selectedHorizontal;
}

[[nodiscard]] skygate::ephemeris::BodyTrailSample
sampleHighPrecisionTrailAtOffset(const SkyObjectTrailInput& input, const int offsetMinutes)
{
    skygate::ephemeris::EphemerisRequest sampleRequest = *input.ephemerisRequest;
    sampleRequest.context.utcTime += std::chrono::minutes(offsetMinutes);
    sampleRequest.epoch = addMinutes(input.ephemerisRequest->epoch, offsetMinutes);

    skygate::ephemeris::BodyTrailSample sample{.offsetMinutes = offsetMinutes, .horizontal = std::nullopt};
    const auto bodyState = input.ephemerisEngine->computeBodyState(sampleRequest, std::size_t{input.targetBodyIndex});
    if (bodyState.has_value() && bodyState->horizontal.isFinite()) {
        sample.horizontal = bodyState->horizontal;
    }
    return sample;
}

[[nodiscard]] bool interpolationErrorWithinBounds(
    const skygate::ephemeris::BodyTrailSample& previous,
    const skygate::ephemeris::BodyTrailSample& midpoint,
    const skygate::ephemeris::BodyTrailSample& next
) noexcept
{
    if (!previous.horizontal.has_value() || !midpoint.horizontal.has_value() || !next.horizontal.has_value()) {
        return true;
    }

    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= 0) {
        return true;
    }

    const skygate::core::HorizontalCoordinate interpolated = interpolateHorizontalCoordinate(
        *previous.horizontal,
        *next.horizontal,
        static_cast<double>(midpoint.offsetMinutes - previous.offsetMinutes) / static_cast<double>(spanMinutes)
    );
    return angularSeparationDegrees(interpolated, *midpoint.horizontal)
           <= kObjectTrailHighPrecisionMaxInterpolationErrorDeg;
}

void appendAdaptiveHighPrecisionAnchors(
    std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const SkyObjectTrailInput& input,
    const skygate::ephemeris::BodyTrailSample& previous,
    const skygate::ephemeris::BodyTrailSample& next
)
{
    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= kObjectTrailHighPrecisionRenderStepMinutes) {
        anchors.push_back(next);
        return;
    }

    const int midpointOffsetMinutes = previous.offsetMinutes + (spanMinutes / 2);
    if (midpointOffsetMinutes <= previous.offsetMinutes || midpointOffsetMinutes >= next.offsetMinutes) {
        anchors.push_back(next);
        return;
    }

    const skygate::ephemeris::BodyTrailSample midpoint = sampleHighPrecisionTrailAtOffset(input, midpointOffsetMinutes);
    if (interpolationErrorWithinBounds(previous, midpoint, next)) {
        anchors.push_back(next);
        return;
    }

    appendAdaptiveHighPrecisionAnchors(anchors, input, previous, midpoint);
    appendAdaptiveHighPrecisionAnchors(anchors, input, midpoint, next);
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample> sampleAdaptiveHighPrecisionTrail(
    const SkyObjectTrailInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const skygate::ephemeris::BodyTrailOptions anchorOptions{
        .pastHours = renderOptions.pastHours,
        .futureHours = renderOptions.futureHours,
        .sampleStepMinutes = kObjectTrailHighPrecisionSampleStepMinutes
    };
    const std::vector<skygate::ephemeris::BodyTrailSample> coarseAnchors =
        trailCalculator.sample(*input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, anchorOptions);
    if (coarseAnchors.size() < 2U) {
        return interpolateTrailSamples(coarseAnchors, renderOptions);
    }

    std::vector<skygate::ephemeris::BodyTrailSample> adaptiveAnchors;
    adaptiveAnchors.reserve(coarseAnchors.size());
    adaptiveAnchors.push_back(coarseAnchors.front());
    for (std::size_t index = 1U; index < coarseAnchors.size(); ++index) {
        appendAdaptiveHighPrecisionAnchors(adaptiveAnchors, input, coarseAnchors[index - 1U], coarseAnchors[index]);
    }

    return interpolateTrailSamples(adaptiveAnchors, renderOptions);
}

[[nodiscard]] std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> sampleGuidanceTrail(
    const SkyObjectTrailInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const std::array<skygate::ephemeris::CelestialBody, 1> bodies{*input.targetBody};
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> guidanceEngine = skygate::ephemeris::createEphemerisEngine(
        std::span<const skygate::ephemeris::CelestialBody>{bodies.data(), bodies.size()}
    );
    if (guidanceEngine == nullptr) {
        return std::nullopt;
    }

    skygate::ephemeris::EphemerisRequest guidanceRequest = *input.ephemerisRequest;
    guidanceRequest.options = guidanceEngine->options();
    guidanceRequest.options.engineKind = guidanceEngine->kind();
    std::vector<skygate::ephemeris::BodyTrailSample> samples =
        trailCalculator.sample(*guidanceEngine, guidanceRequest, 0U, renderOptions);
    alignGuidanceTrailToSelectedState(samples, input);
    return samples;
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample>
sampleFixedEquatorialTrail(const SkyObjectTrailInput& input, const skygate::ephemeris::BodyTrailOptions& renderOptions)
{
    const skygate::core::SkyContext& context =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    const skygate::core::EquatorialCoordinate equatorial = input.targetState->equatorial;
    const int startOffsetMinutes = -renderOptions.pastHours * 60;
    const int endOffsetMinutes = renderOptions.futureHours * 60;

    std::vector<skygate::ephemeris::BodyTrailSample> samples;
    samples.reserve(
        static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / renderOptions.sampleStepMinutes) + 1U
    );

    for (int offsetMinutes = startOffsetMinutes; offsetMinutes <= endOffsetMinutes;
         offsetMinutes += renderOptions.sampleStepMinutes) {
        const skygate::core::UtcTimePoint sampleTime = context.utcTime + std::chrono::minutes(offsetMinutes);
        samples.push_back(
            skygate::ephemeris::BodyTrailSample{
                .offsetMinutes = offsetMinutes,
                .horizontal = skygate::ephemeris::CelestialReferenceCalculator::equatorialPoint(
                    equatorial.rightAscensionHours, equatorial.declinationDeg, context.observer, sampleTime
                )
            }
        );
    }

    return samples;
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample> sampleTrail(
    const SkyObjectTrailInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    if (isFixedEquatorialTrailTarget(input)) {
        return sampleFixedEquatorialTrail(input, renderOptions);
    }

    if (shouldUseGuidanceTrail(input)) {
        if (std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> guidanceSamples =
                sampleGuidanceTrail(input, trailCalculator, renderOptions);
            guidanceSamples.has_value()) {
            return std::move(*guidanceSamples);
        }
    }

    if (usesHighPrecisionRequest(input)) {
        return sampleAdaptiveHighPrecisionTrail(input, trailCalculator, renderOptions);
    }

    return input.ephemerisRequest.has_value()
               ? trailCalculator.sample(
                     *input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, renderOptions
                 )
               : trailCalculator.sample(*input.ephemerisEngine, input.skyContext, input.targetBodyIndex, renderOptions);
}

void appendTrailLine(
    SkyRenderFrame& frame,
    const double x1,
    const double y1,
    const double x2,
    const double y2,
    const double widthPx,
    const QColor& color
)
{
    frame.lines.push_back(SkyRenderLine{.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2, .widthPx = widthPx, .color = color});
}

void appendDashedTrailLine(
    SkyRenderFrame& frame,
    const double x1,
    const double y1,
    const double x2,
    const double y2,
    const double widthPx,
    const QColor& color
)
{
    const skygate::core::DashedLineBuilder dashBuilder;
    for (const auto& dash : dashBuilder.build(
             skygate::core::LineSegment2d{.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2},
             kObjectTrailPastDashLengthPx,
             kObjectTrailPastDashGapPx
         )) {
        appendTrailLine(frame, dash.x1, dash.y1, dash.x2, dash.y2, widthPx, color);
    }
}

void appendFutureTrailTick(
    SkyRenderFrame& frame, const skygate::core::ScreenPoint& point, const int offsetMinutes, const QColor& color
)
{
    appendTrailLine(
        frame,
        point.x - kObjectTrailFutureTickRadiusPx,
        point.y,
        point.x + kObjectTrailFutureTickRadiusPx,
        point.y,
        kObjectTrailFutureWidthPx,
        color
    );
    appendTrailLine(
        frame,
        point.x,
        point.y - kObjectTrailFutureTickRadiusPx,
        point.x,
        point.y + kObjectTrailFutureTickRadiusPx,
        kObjectTrailFutureWidthPx,
        color
    );

    skygate::ui::internal::appendSkyRenderLabel(
        frame.labels, "trailTick", point.x, point.y, QString("+%1h").arg(offsetMinutes / 60), color
    );
}

}  // namespace

SkyObjectTrailBuilder::TrailSampleCacheKey SkyObjectTrailBuilder::sampleCacheKeyFor(const SkyObjectTrailInput& input)
{
    const skygate::core::SkyContext& context =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    return TrailSampleCacheKey{
        .ephemerisEngine = input.ephemerisEngine,
        .context = context,
        .requestEpoch = input.ephemerisRequest.has_value() ? std::make_optional(input.ephemerisRequest->epoch)
                                                           : std::optional<skygate::ephemeris::AstronomicalEpoch>{},
        .requestOptions = input.ephemerisRequest.has_value()
                              ? std::make_optional(input.ephemerisRequest->options)
                              : std::optional<skygate::ephemeris::EphemerisEngineOptions>{},
        .targetEquatorial = input.targetState != nullptr && input.targetState->equatorial.isFinite()
                                ? std::make_optional(input.targetState->equatorial)
                                : std::optional<skygate::core::EquatorialCoordinate>{},
        .targetBodyIndex = input.targetBodyIndex
    };
}

bool SkyObjectTrailBuilder::sampleCacheKeysEqual(
    const TrailSampleCacheKey& lhs, const TrailSampleCacheKey& rhs
) noexcept
{
    return lhs.ephemerisEngine == rhs.ephemerisEngine && lhs.targetBodyIndex == rhs.targetBodyIndex
           && lhs.context.utcTime == rhs.context.utcTime && observersEqual(lhs.context.observer, rhs.context.observer)
           && optionalEpochsEqual(lhs.requestEpoch, rhs.requestEpoch)
           && optionalOptionsEqual(lhs.requestOptions, rhs.requestOptions)
           && optionalEquatorialsEqual(lhs.targetEquatorial, rhs.targetEquatorial);
}

const std::vector<skygate::ephemeris::BodyTrailSample>& SkyObjectTrailBuilder::trailSamples(
    const SkyObjectTrailInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& trailOptions
) const
{
    const TrailSampleCacheKey cacheKey = sampleCacheKeyFor(input);
    if (!m_sampleCacheKey.has_value() || !sampleCacheKeysEqual(*m_sampleCacheKey, cacheKey)) {
        m_sampleCache = sampleTrail(input, trailCalculator, trailOptions);
        m_sampleCacheKey = cacheKey;
    }

    return m_sampleCache;
}

void SkyObjectTrailBuilder::appendTrail(SkyRenderFrame& frame, const SkyObjectTrailInput& input) const
{
    QElapsedTimer timer;
    skygate::ui::startPerformanceTimer(timer);

    const skygate::core::SkyContext& context =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    if (input.ephemerisEngine == nullptr || input.preparedProjection == nullptr || !context.observer.isValid()) {
        return;
    }

    const double maxSegmentLength = std::max(input.viewportWidth, input.viewportHeight) * 0.35;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;
    const QColor pastColor = colorWithAlpha(input.renderTheme.selectionMarkerBorder, 105);
    const QColor futureColor = colorWithAlpha(input.renderTheme.selectionMarkerBorder, 175);
    const skygate::core::ProjectedPolylineBuilder polylineBuilder;

    bool hasPreviousCoordinate = false;
    skygate::core::HorizontalCoordinate previousCoordinate;
    int previousOffsetMinutes = 0;

    const skygate::ephemeris::BodyTrailCalculator trailCalculator;
    const skygate::ephemeris::BodyTrailOptions trailOptions{
        .pastHours = kObjectTrailPastHours,
        .futureHours = kObjectTrailFutureHours,
        .sampleStepMinutes =
            usesHighPrecisionRequest(input) ? kObjectTrailHighPrecisionRenderStepMinutes : kObjectTrailSampleStepMinutes
    };
    const TrailSampleCacheKey cacheKey = sampleCacheKeyFor(input);
    const bool sampleCacheHit = m_sampleCacheKey.has_value() && sampleCacheKeysEqual(*m_sampleCacheKey, cacheKey);
    const auto& samples = trailSamples(input, trailCalculator, trailOptions);
    const qint64 sampleNs = skygate::ui::performanceElapsedNanoseconds(timer);

    int lineCountBefore = 0;
    int labelCountBefore = 0;
    if (skygate::ui::performanceLoggingEnabled()) {
        lineCountBefore = static_cast<int>(frame.lines.size());
        labelCountBefore = static_cast<int>(frame.labels.size());
    }

    for (const auto& sample : samples) {
        if (!sample.horizontal.has_value() || !sample.horizontal->isValid()) {
            hasPreviousCoordinate = false;
            continue;
        }

        if (hasPreviousCoordinate) {
            const std::array<skygate::core::HorizontalCoordinate, 2> coordinates{
                previousCoordinate, *sample.horizontal
            };
            const bool isPastSegment = previousOffsetMinutes < 0 && sample.offsetMinutes <= 0;
            for (const auto& segment :
                 polylineBuilder.build(*input.preparedProjection, coordinates, maxSegmentLengthSquared)) {
                if (isPastSegment) {
                    appendDashedTrailLine(
                        frame, segment.x1, segment.y1, segment.x2, segment.y2, kObjectTrailPastWidthPx, pastColor
                    );
                } else {
                    appendTrailLine(
                        frame, segment.x1, segment.y1, segment.x2, segment.y2, kObjectTrailFutureWidthPx, futureColor
                    );
                }
            }
        }

        const auto projected = input.preparedProjection->project(*sample.horizontal);
        if (projected.isVisible && projected.isFinite() && sample.offsetMinutes > 0
            && sample.offsetMinutes % (kObjectTrailFutureTickStepHours * 60) == 0) {
            appendFutureTrailTick(frame, projected, sample.offsetMinutes, futureColor);
        }

        previousCoordinate = *sample.horizontal;
        previousOffsetMinutes = sample.offsetMinutes;
        hasPreviousCoordinate = true;
    }

    if (skygate::ui::performanceLoggingEnabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "trail append elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer)
            << "sampleMs=" << skygate::ui::performanceMilliseconds(sampleNs)
            << "renderMs=" << skygate::ui::performanceMilliseconds(timer.nsecsElapsed() - sampleNs)
            << "samples=" << static_cast<qsizetype>(samples.size()) << "cacheHit=" << sampleCacheHit
            << "highPrecision=" << usesHighPrecisionRequest(input) << "guidance=" << shouldUseGuidanceTrail(input)
            << "fixedEquatorial=" << isFixedEquatorialTrailTarget(input)
            << "bodyIndex=" << static_cast<qulonglong>(input.targetBodyIndex)
            << "linesAdded=" << static_cast<int>(frame.lines.size()) - lineCountBefore
            << "labelsAdded=" << static_cast<int>(frame.labels.size()) - labelCountBefore;
    }
}
