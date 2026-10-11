#include "SkyObjectTrailSampler.hpp"
#include "CelestialBodyCatalog.hpp"
#include "engine/EphemerisComputationPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "math/SphericalGeometry.hpp"
#include "math/Vector3d.hpp"
#include "trail/BodyTrailCalculator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <vector>

namespace {

constexpr int kObjectTrailPastHours = 6;
constexpr int kObjectTrailFutureHours = 18;
constexpr int kObjectTrailSampleStepMinutes = 30;
constexpr int kObjectTrailAdaptiveRenderStepMinutes = 10;
constexpr int kObjectTrailAdaptiveSampleStepMinutes = 120;
constexpr double kObjectTrailAdaptiveMaxInterpolationErrorDeg = 0.05;

void copyCommonBodyFields(
    skygate::ephemeris::BaseCelestialBody& target, const skygate::ephemeris::BaseCelestialBody& source
)
{
    target.id = source.id;
    target.displayName = source.displayName;
    target.kind = source.kind;
    target.visualMagnitude = source.visualMagnitude;
}

[[nodiscard]] skygate::ephemeris::CelestialBodyCatalog
makeSingleBodyCatalog(const skygate::ephemeris::BaseCelestialBody& body)
{
    if (body.kind == skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject) {
        skygate::ephemeris::DistantCelestialBody distantBody;
        copyCommonBodyFields(distantBody, body);
        distantBody.fixedEquatorial = body.fixedEquatorialValue();
        distantBody.deepSkyObject = body.deepSkyObjectValue();
        return skygate::ephemeris::CelestialBodyCatalog(
            {},
            {std::move(distantBody)},
            {{.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = 0U}}
        );
    }

    skygate::ephemeris::OwnGalaxyCelestialBody ownGalaxyBody;
    copyCommonBodyFields(ownGalaxyBody, body);
    ownGalaxyBody.fixedEquatorial = body.fixedEquatorialValue();
    ownGalaxyBody.starAstrometry = body.starAstrometryValue();
    return skygate::ephemeris::CelestialBodyCatalog({std::move(ownGalaxyBody)});
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

[[nodiscard]] skygate::core::Vector3d perpendicularAxis(const skygate::core::Vector3d& vector) noexcept
{
    const skygate::core::Vector3d zAxis{.x = 0.0, .y = 0.0, .z = 1.0};
    skygate::core::Vector3d axis = vector.cross(zAxis);
    if (axis.length() <= 1e-9) {
        axis = vector.cross(skygate::core::Vector3d{.x = 1.0, .y = 0.0, .z = 0.0});
    }
    return axis.normalized().value_or(skygate::core::Vector3d{.x = 0.0, .y = 0.0, .z = 1.0});
}

[[nodiscard]] skygate::core::Vector3d rotatedAroundAxis(
    const skygate::core::Vector3d& vector, const skygate::core::Vector3d& axis, const double angleRad
) noexcept
{
    const double cosAngle = std::cos(angleRad);
    const double sinAngle = std::sin(angleRad);
    const skygate::core::Vector3d axisCrossVector = axis.cross(vector);
    const double axisDotVector = axis.dot(vector);
    const skygate::core::Vector3d rotated =
        (vector * cosAngle) + (axisCrossVector * sinAngle) + (axis * (axisDotVector * (1.0 - cosAngle)));
    return rotated.normalized().value_or(skygate::core::Vector3d{.x = 0.0, .y = 0.0, .z = 1.0});
}

[[nodiscard]] double angularSeparationDegrees(
    const skygate::core::HorizontalCoordinate& lhs, const skygate::core::HorizontalCoordinate& rhs
) noexcept
{
    constexpr double radiansToDegrees = 180.0 / std::numbers::pi;
    const skygate::core::Vector3d lhsVector =
        skygate::core::SphericalGeometry::horizontalToUnitVector(lhs.normalizedAzimuth());
    const skygate::core::Vector3d rhsVector =
        skygate::core::SphericalGeometry::horizontalToUnitVector(rhs.normalizedAzimuth());
    const double dot = std::clamp(lhsVector.dot(rhsVector), -1.0, 1.0);
    return std::acos(dot) * radiansToDegrees;
}

[[nodiscard]] skygate::core::HorizontalCoordinate interpolateHorizontalCoordinate(
    const skygate::core::HorizontalCoordinate& previous,
    const skygate::core::HorizontalCoordinate& next,
    const double fraction
) noexcept
{
    const skygate::core::Vector3d previousVector = skygate::core::SphericalGeometry::horizontalToUnitVector(previous);
    const skygate::core::Vector3d nextVector = skygate::core::SphericalGeometry::horizontalToUnitVector(next);
    const skygate::core::Vector3d interpolated = previousVector + ((nextVector - previousVector) * fraction);
    return skygate::core::SphericalGeometry::horizontalFromUnitVector(
        interpolated.normalized().value_or(skygate::core::Vector3d{.x = 0.0, .y = 0.0, .z = 1.0})
    );
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

void alignTrailToSelectedState(
    std::vector<skygate::ephemeris::BodyTrailSample>& samples, const SkyObjectTrailSamplingInput& input
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
    const skygate::core::Vector3d guidanceNow =
        skygate::core::SphericalGeometry::horizontalToUnitVector(*presentSample->horizontal);
    const skygate::core::Vector3d selectedNow =
        skygate::core::SphericalGeometry::horizontalToUnitVector(selectedHorizontal);
    const double dot = std::clamp(guidanceNow.dot(selectedNow), -1.0, 1.0);
    skygate::core::Vector3d axis = guidanceNow.cross(selectedNow);
    const double axisLength = axis.length();
    double angleRad = 0.0;
    if (axisLength <= 1e-9) {
        if (dot > 0.0) {
            presentSample->horizontal = selectedHorizontal;
            return;
        }
        axis = perpendicularAxis(guidanceNow);
        angleRad = std::numbers::pi;
    } else {
        axis = axis.normalized().value_or(skygate::core::Vector3d{.x = 0.0, .y = 0.0, .z = 1.0});
        angleRad = std::atan2(axisLength, dot);
    }

    for (skygate::ephemeris::BodyTrailSample& sample : samples) {
        if (sample.horizontal.has_value() && sample.horizontal->isFinite()) {
            sample.horizontal = skygate::core::SphericalGeometry::horizontalFromUnitVector(rotatedAroundAxis(
                skygate::core::SphericalGeometry::horizontalToUnitVector(*sample.horizontal), axis, angleRad
            ));
        }
    }
    presentSample->horizontal = selectedHorizontal;
}

[[nodiscard]] skygate::ephemeris::BodyTrailSample
sampleEngineTrailAtOffset(const SkyObjectTrailSamplingInput& input, const int offsetMinutes)
{
    skygate::ephemeris::EphemerisRequest sampleRequest = *input.ephemerisRequest;
    sampleRequest.context.utcTime += std::chrono::minutes(offsetMinutes);
    sampleRequest.epoch = input.ephemerisRequest->epoch.addMinutes(offsetMinutes);

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
    return angularSeparationDegrees(interpolated, *midpoint.horizontal) <= kObjectTrailAdaptiveMaxInterpolationErrorDeg;
}

void appendAdaptiveTrailAnchors(
    std::vector<skygate::ephemeris::BodyTrailSample>& anchors,
    const SkyObjectTrailSamplingInput& input,
    const skygate::ephemeris::BodyTrailSample& previous,
    const skygate::ephemeris::BodyTrailSample& next
)
{
    const int spanMinutes = next.offsetMinutes - previous.offsetMinutes;
    if (spanMinutes <= kObjectTrailAdaptiveRenderStepMinutes) {
        anchors.push_back(next);
        return;
    }

    const int midpointOffsetMinutes = previous.offsetMinutes + (spanMinutes / 2);
    if (midpointOffsetMinutes <= previous.offsetMinutes || midpointOffsetMinutes >= next.offsetMinutes) {
        anchors.push_back(next);
        return;
    }

    const skygate::ephemeris::BodyTrailSample midpoint = sampleEngineTrailAtOffset(input, midpointOffsetMinutes);
    if (interpolationErrorWithinBounds(previous, midpoint, next)) {
        anchors.push_back(next);
        return;
    }

    appendAdaptiveTrailAnchors(anchors, input, previous, midpoint);
    appendAdaptiveTrailAnchors(anchors, input, midpoint, next);
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample> sampleAdaptiveTrail(
    const SkyObjectTrailSamplingInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const skygate::ephemeris::BodyTrailOptions anchorOptions{
        .pastHours = renderOptions.pastHours,
        .futureHours = renderOptions.futureHours,
        .sampleStepMinutes = kObjectTrailAdaptiveSampleStepMinutes
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
        appendAdaptiveTrailAnchors(adaptiveAnchors, input, coarseAnchors[index - 1U], coarseAnchors[index]);
    }

    return interpolateTrailSamples(adaptiveAnchors, renderOptions);
}

[[nodiscard]] std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> sampleGuidanceTrail(
    const SkyObjectTrailSamplingInput& input,
    const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
    const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const skygate::ephemeris::CelestialBodyCatalog catalog = makeSingleBodyCatalog(*input.targetBody);
    if (input.guidanceStrategy == nullptr) {
        return std::nullopt;
    }

    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> guidanceEngine =
        input.guidanceStrategy->createGuidanceEngine(catalog);
    if (guidanceEngine == nullptr) {
        return std::nullopt;
    }

    skygate::ephemeris::EphemerisRequest guidanceRequest = *input.ephemerisRequest;
    guidanceRequest.options = guidanceEngine->options();
    guidanceRequest.options.setEngineKind(guidanceEngine->kind());
    std::vector<skygate::ephemeris::BodyTrailSample> samples =
        trailCalculator.sample(*guidanceEngine, guidanceRequest, 0U, renderOptions);
    alignTrailToSelectedState(samples, input);
    return samples;
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample> sampleFixedEquatorialTrail(
    const SkyObjectTrailSamplingInput& input, const skygate::ephemeris::BodyTrailOptions& renderOptions
)
{
    const int startOffsetMinutes = -renderOptions.pastHours * 60;
    const int endOffsetMinutes = renderOptions.futureHours * 60;

    std::vector<skygate::ephemeris::BodyTrailSample> samples;
    samples.reserve(
        static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / renderOptions.sampleStepMinutes) + 1U
    );

    for (int offsetMinutes = startOffsetMinutes; offsetMinutes <= endOffsetMinutes;
         offsetMinutes += renderOptions.sampleStepMinutes) {
        samples.push_back(sampleEngineTrailAtOffset(input, offsetMinutes));
    }

    alignTrailToSelectedState(samples, input);
    return samples;
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample>
sampleTrail(const SkyObjectTrailSamplingInput& input, const skygate::ephemeris::BodyTrailCalculator& trailCalculator)
{
    const skygate::ephemeris::BodyTrailOptions renderOptions{
        .pastHours = kObjectTrailPastHours,
        .futureHours = kObjectTrailFutureHours,
        .sampleStepMinutes = SkyObjectTrailSampler::usesAdaptiveTrailSampling(input)
                                 ? kObjectTrailAdaptiveRenderStepMinutes
                                 : kObjectTrailSampleStepMinutes
    };

    if (SkyObjectTrailSampler::isFixedEquatorialTrailTarget(input)) {
        return sampleFixedEquatorialTrail(input, renderOptions);
    }

    if (SkyObjectTrailSampler::shouldUseGuidanceTrail(input)) {
        if (std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> guidanceSamples =
                sampleGuidanceTrail(input, trailCalculator, renderOptions);
            guidanceSamples.has_value()) {
            return std::move(*guidanceSamples);
        }
    }

    if (SkyObjectTrailSampler::usesAdaptiveTrailSampling(input)) {
        return sampleAdaptiveTrail(input, trailCalculator, renderOptions);
    }

    return input.ephemerisRequest.has_value()
               ? trailCalculator.sample(
                     *input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, renderOptions
                 )
               : trailCalculator.sample(*input.ephemerisEngine, input.skyContext, input.targetBodyIndex, renderOptions);
}

}  // namespace

bool SkyObjectTrailSampler::usesAdaptiveTrailSampling(const SkyObjectTrailSamplingInput& input) noexcept
{
    return input.ephemerisEngine != nullptr && input.ephemerisRequest.has_value()
           && skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(
               *input.ephemerisEngine, *input.ephemerisRequest
           );
}

bool SkyObjectTrailSampler::isFixedEquatorialTrailTarget(const SkyObjectTrailSamplingInput& input) noexcept
{
    if (!usesAdaptiveTrailSampling(input) || input.ephemerisEngine == nullptr
        || !skygate::ephemeris::EphemerisComputationPolicy::trailAllowsGuidanceApproximation(*input.ephemerisEngine)
        || input.targetBody == nullptr || input.targetState == nullptr || !input.targetState->equatorial.isFinite()) {
        return false;
    }

    return input.targetBody->fixedEquatorialValue().has_value() || input.targetBody->starAstrometryValue().has_value()
           || input.targetBody->kind == skygate::ephemeris::BaseCelestialBody::Kind::Star;
}

bool SkyObjectTrailSampler::shouldUseGuidanceTrail(const SkyObjectTrailSamplingInput& input) noexcept
{
    return input.ephemerisEngine != nullptr && input.ephemerisRequest.has_value()
           && skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(
               *input.ephemerisEngine, *input.ephemerisRequest, input.targetBody
           )
           && !isFixedEquatorialTrailTarget(input);
}

SkyObjectTrailSampler::TrailSampleCacheKey
SkyObjectTrailSampler::sampleCacheKeyFor(const SkyObjectTrailSamplingInput& input)
{
    const skygate::core::ObservationContext& context =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    return TrailSampleCacheKey{
        .ephemerisEngine = input.ephemerisEngine,
        .guidanceStrategy = input.guidanceStrategy,
        .context = context,
        .requestEpoch = input.ephemerisRequest.has_value() ? std::make_optional(input.ephemerisRequest->epoch)
                                                           : std::optional<skygate::core::AstronomicalEpoch>{},
        .requestOptions = input.ephemerisRequest.has_value()
                              ? std::make_optional(input.ephemerisRequest->options)
                              : std::optional<skygate::ephemeris::EphemerisEngineOptions>{},
        .targetEquatorial = input.targetState != nullptr && input.targetState->equatorial.isFinite()
                                ? std::make_optional(input.targetState->equatorial)
                                : std::optional<skygate::core::EquatorialCoordinate>{},
        .targetBodyIndex = input.targetBodyIndex
    };
}

bool SkyObjectTrailSampler::sampleCacheKeysEqual(
    const TrailSampleCacheKey& lhs, const TrailSampleCacheKey& rhs
) noexcept
{
    return lhs.ephemerisEngine == rhs.ephemerisEngine && lhs.guidanceStrategy == rhs.guidanceStrategy
           && lhs.targetBodyIndex == rhs.targetBodyIndex && lhs.context.utcTime == rhs.context.utcTime
           && observersEqual(lhs.context.observer, rhs.context.observer) && lhs.requestEpoch == rhs.requestEpoch
           && lhs.requestOptions == rhs.requestOptions
           && optionalEquatorialsEqual(lhs.targetEquatorial, rhs.targetEquatorial);
}

bool SkyObjectTrailSampler::hasCachedSamples(const SkyObjectTrailSamplingInput& input) const
{
    return m_sampleCacheKey.has_value() && sampleCacheKeysEqual(*m_sampleCacheKey, sampleCacheKeyFor(input));
}

const std::vector<skygate::ephemeris::BodyTrailSample>&
SkyObjectTrailSampler::samples(const SkyObjectTrailSamplingInput& input)
{
    const TrailSampleCacheKey cacheKey = sampleCacheKeyFor(input);
    if (!m_sampleCacheKey.has_value() || !sampleCacheKeysEqual(*m_sampleCacheKey, cacheKey)) {
        const skygate::ephemeris::BodyTrailCalculator trailCalculator;
        m_sampleCache = sampleTrail(input, trailCalculator);
        m_sampleCacheKey = cacheKey;
    }

    return m_sampleCache;
}
