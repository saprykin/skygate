#include "ObservationEventSearch.hpp"
#include "BaseCelestialBody.hpp"
#include "EphemerisRequestFactory.hpp"
#include "FixedBodyAltitudeRange.hpp"
#include "UtcTimeCodec.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "math/TimeConstants.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace skygate::ephemeris {
namespace {

using AltitudeSample = ObservationEventSearch::AltitudeSample;

constexpr int kSampleStepSeconds = static_cast<int>(10.0 * skygate::core::TimeConstants::kSecondsPerMinute);
constexpr int kSearchHorizonSeconds =
    ObservationEventSearch::kSearchHorizonHours * static_cast<int>(skygate::core::TimeConstants::kSecondsPerHour);
constexpr auto kRefinementTolerance = std::chrono::seconds(1);
constexpr auto kGuidedRefinementTolerance = std::chrono::seconds(60);
constexpr double kAltitudeClassificationToleranceDeg = 1e-9;

[[nodiscard]] bool
isRiseBracket(const AltitudeSample& previous, const AltitudeSample& next, double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg < crossingAltitudeDeg && next.altitudeDeg >= crossingAltitudeDeg;
}

[[nodiscard]] bool
isSetBracket(const AltitudeSample& previous, const AltitudeSample& next, double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg > crossingAltitudeDeg && next.altitudeDeg <= crossingAltitudeDeg;
}

[[nodiscard]] bool isCrossingBracket(
    const AltitudeSample& previous, const AltitudeSample& next, bool rising, double crossingAltitudeDeg
) noexcept
{
    return rising ? isRiseBracket(previous, next, crossingAltitudeDeg)
                  : isSetBracket(previous, next, crossingAltitudeDeg);
}

[[nodiscard]] skygate::core::UtcTimePoint interpolatedCrossingTime(
    const AltitudeSample& previous, const AltitudeSample& next, double crossingAltitudeDeg
) noexcept
{
    const double altitudeSpanDeg = next.altitudeDeg - previous.altitudeDeg;
    if (altitudeSpanDeg == 0.0 || !std::isfinite(altitudeSpanDeg)) {
        return next.utcTime;
    }

    const double fraction = std::clamp((crossingAltitudeDeg - previous.altitudeDeg) / altitudeSpanDeg, 0.0, 1.0);
    const std::chrono::duration<double> spanSeconds = next.utcTime - previous.utcTime;
    return previous.utcTime + std::chrono::duration_cast<skygate::core::UtcTimePoint::duration>(spanSeconds * fraction);
}

[[nodiscard]] ObservationEventStatus::Type unavailableEventStatus(
    const std::vector<AltitudeSample>& samples, std::optional<ObservationEventStatus::Type> provenFixedStatus
) noexcept
{
    if (samples.empty()) {
        return ObservationEventStatus::Type::Unresolved;
    }
    if (provenFixedStatus.has_value()) {
        return *provenFixedStatus;
    }
    return ObservationEventStatus::Type::NoEventInSearchWindow;
}

}  // namespace

ObservationEventSearch::ObservationEventSearch(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const BaseCelestialBody* body,
    const double crossingAltitudeDeg
) noexcept
    : m_ephemerisEngine(ephemerisEngine), m_request(request), m_bodyIndex(bodyIndex), m_body(body),
      m_crossingAltitudeDeg(crossingAltitudeDeg)
{
}

std::optional<double> ObservationEventSearch::altitudeAt(const skygate::core::UtcTimePoint& utcTime)
{
    const auto request = EphemerisRequestFactory::atUtcTime(m_request, utcTime);
    const auto state = m_ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(m_bodyIndex));
    if (!state.has_value() || !state->horizontal.isFinite()) {
        return std::nullopt;
    }
    return state->horizontal.altitudeDeg;
}

std::vector<AltitudeSample> ObservationEventSearch::sampleAltitudes(
    const IEphemerisEngine& engine, const EphemerisRequest& request, const std::uint32_t bodyIndex
)
{
    std::vector<AltitudeSample> samples;
    samples.reserve((kSearchHorizonSeconds / kSampleStepSeconds) + 1);

    for (int offsetSeconds = 0; offsetSeconds <= kSearchHorizonSeconds; offsetSeconds += kSampleStepSeconds) {
        const skygate::core::UtcTimePoint utcTime = request.context.utcTime + std::chrono::seconds(offsetSeconds);
        const auto sampleRequest = EphemerisRequestFactory::atUtcTime(request, utcTime);
        const auto state = engine.computeBodyState(sampleRequest, static_cast<std::size_t>(bodyIndex));
        if (!state.has_value() || !state->horizontal.isFinite()) {
            continue;
        }
        samples.push_back(AltitudeSample{.utcTime = utcTime, .altitudeDeg = state->horizontal.altitudeDeg});
    }

    return samples;
}

std::optional<ObservationEventStatus::Type> ObservationEventSearch::fixedHorizonStatus() const noexcept
{
    if (m_body == nullptr || !m_body->hasFixedEquatorial()) {
        return std::nullopt;
    }

    const skygate::core::EquatorialCoordinate& equatorial = *m_body->fixedEquatorialValue();
    if (!equatorial.isFinite()) {
        return std::nullopt;
    }

    const double maxAltitudeDeg =
        FixedBodyAltitudeRange::maxAltitudeDeg(m_request.context.observer.latitudeDeg, equatorial.declinationDeg);
    const double minAltitudeDeg =
        FixedBodyAltitudeRange::minAltitudeDeg(m_request.context.observer.latitudeDeg, equatorial.declinationDeg);

    if (minAltitudeDeg >= m_crossingAltitudeDeg - kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::Type::AlwaysAbove;
    }
    if (maxAltitudeDeg <= m_crossingAltitudeDeg + kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::Type::AlwaysBelow;
    }

    return std::nullopt;
}

std::optional<std::array<AltitudeSample, 2>>
ObservationEventSearch::originalAltitudeBracket(const AltitudeSample& previous, const AltitudeSample& next)
{
    const auto previousAltitude = altitudeAt(previous.utcTime);
    const auto nextAltitude = altitudeAt(next.utcTime);
    if (!previousAltitude.has_value() || !nextAltitude.has_value()) {
        return std::nullopt;
    }

    return std::array<AltitudeSample, 2>{
        AltitudeSample{.utcTime = previous.utcTime, .altitudeDeg = *previousAltitude},
        AltitudeSample{.utcTime = next.utcTime, .altitudeDeg = *nextAltitude},
    };
}

skygate::core::UtcTimePoint ObservationEventSearch::refinedCrossingTime(
    const AltitudeSample& previous,
    const AltitudeSample& next,
    const bool rising,
    const skygate::core::UtcTimePoint::duration refinementTolerance
)
{
    skygate::core::UtcTimePoint low = previous.utcTime;
    skygate::core::UtcTimePoint high = next.utcTime;

    while (high - low > refinementTolerance) {
        const auto midpointOffset = (high - low) / 2;
        const skygate::core::UtcTimePoint midpoint = low + midpointOffset;
        const auto midpointAltitude = altitudeAt(midpoint);
        if (!midpointAltitude.has_value()) {
            break;
        }

        if (rising) {
            if (*midpointAltitude >= m_crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        } else {
            if (*midpointAltitude <= m_crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        }
    }

    return high;
}

ObservationEvent ObservationEventSearch::findCrossing(
    const std::vector<AltitudeSample>& samples,
    const SampleRole sampleRole,
    const bool trustGuidance,
    const bool rising,
    const std::optional<ObservationEventStatus::Type> provenFixedStatus
)
{
    bool rejectedGuidanceBracket = false;

    for (std::size_t index = 1; index < samples.size(); ++index) {
        AltitudeSample previous = samples[index - 1U];
        AltitudeSample next = samples[index];
        if (!isCrossingBracket(previous, next, rising, m_crossingAltitudeDeg)) {
            continue;
        }

        if (sampleRole == SampleRole::Guidance && !trustGuidance) {
            const auto originalBracket = originalAltitudeBracket(previous, next);
            if (!originalBracket.has_value()
                || !isCrossingBracket((*originalBracket)[0], (*originalBracket)[1], rising, m_crossingAltitudeDeg)) {
                rejectedGuidanceBracket = true;
                continue;
            }
            previous = (*originalBracket)[0];
            next = (*originalBracket)[1];
        }

        const skygate::core::UtcTimePoint crossingUtcTime =
            trustGuidance ? interpolatedCrossingTime(previous, next, m_crossingAltitudeDeg)
                          : refinedCrossingTime(
                                previous,
                                next,
                                rising,
                                sampleRole == SampleRole::Guidance ? kGuidedRefinementTolerance : kRefinementTolerance
                            );

        return ObservationEvent{
            .status = ObservationEventStatus::Type::Available,
            .utcTime = crossingUtcTime,
            .altitudeDeg = m_crossingAltitudeDeg
        };
    }

    if (rejectedGuidanceBracket) {
        return ObservationEvent{.status = ObservationEventStatus::Type::Unresolved};
    }

    return ObservationEvent{.status = unavailableEventStatus(samples, provenFixedStatus)};
}

AltitudeSample ObservationEventSearch::refinedMaximum(
    const skygate::core::UtcTimePoint& startUtc, const skygate::core::UtcTimePoint& endUtc
)
{
    auto lowSeconds = skygate::core::UtcTimeCodec::toEpochSecondsFloor(startUtc);
    auto highSeconds = skygate::core::UtcTimeCodec::toEpochSecondsFloor(endUtc);

    while (highSeconds - lowSeconds > 3) {
        const auto spanSeconds = highSeconds - lowSeconds;
        const auto firstSeconds = lowSeconds + spanSeconds / 3;
        const auto secondSeconds = highSeconds - spanSeconds / 3;
        const skygate::core::UtcTimePoint firstUtc = skygate::core::UtcTimeCodec::fromEpochSeconds(firstSeconds);
        const skygate::core::UtcTimePoint secondUtc = skygate::core::UtcTimeCodec::fromEpochSeconds(secondSeconds);
        const auto firstAltitude = altitudeAt(firstUtc);
        const auto secondAltitude = altitudeAt(secondUtc);
        if (!firstAltitude.has_value() || !secondAltitude.has_value()) {
            break;
        }

        if (*firstAltitude < *secondAltitude) {
            lowSeconds = firstSeconds;
        } else {
            highSeconds = secondSeconds;
        }
    }

    AltitudeSample best{.utcTime = startUtc, .altitudeDeg = -std::numeric_limits<double>::infinity()};
    for (auto seconds = lowSeconds; seconds <= highSeconds; ++seconds) {
        const skygate::core::UtcTimePoint utcTime = skygate::core::UtcTimeCodec::fromEpochSeconds(seconds);
        const auto altitude = altitudeAt(utcTime);
        if (altitude.has_value() && *altitude > best.altitudeDeg) {
            best.utcTime = utcTime;
            best.altitudeDeg = *altitude;
        }
    }

    return best;
}

ObservationEvent
ObservationEventSearch::findCulmination(const std::vector<AltitudeSample>& samples, const bool trustGuidance)
{
    for (std::size_t index = 1; index + 1U < samples.size(); ++index) {
        const AltitudeSample& previous = samples[index - 1U];
        const AltitudeSample& current = samples[index];
        const AltitudeSample& next = samples[index + 1U];

        if (current.altitudeDeg < previous.altitudeDeg || current.altitudeDeg < next.altitudeDeg) {
            continue;
        }

        const AltitudeSample maximum = trustGuidance ? current : refinedMaximum(previous.utcTime, next.utcTime);
        if (!std::isfinite(maximum.altitudeDeg)) {
            break;
        }

        return ObservationEvent{
            .status = ObservationEventStatus::Type::Available,
            .utcTime = maximum.utcTime,
            .altitudeDeg = maximum.altitudeDeg
        };
    }

    return ObservationEvent{.status = unavailableEventStatus(samples, std::nullopt)};
}

}  // namespace skygate::ephemeris
