#include "skygate/ephemeris/ObservationEventCalculator.hpp"

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace skygate::ephemeris {
namespace {

constexpr int kSampleStepSeconds = 10 * 60;
constexpr int kSearchHorizonSeconds = 72 * 60 * 60;
constexpr int kRefinementToleranceSeconds = 1;
constexpr double kAltitudeClassificationToleranceDeg = 1e-9;
constexpr double kUnixEpochJulianDay = 2'440'587.5;
constexpr double kSecondsPerDay = 86'400.0;

struct AltitudeSample final {
    core::UtcTimePoint utcTime;
    double altitudeDeg = std::numeric_limits<double>::quiet_NaN();
};

[[nodiscard]] core::UtcTimePoint addSeconds(const core::UtcTimePoint& utcTime, const int seconds) noexcept
{
    return utcTime + std::chrono::seconds(seconds);
}

[[nodiscard]] AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay =
        kUnixEpochJulianDay + static_cast<double>(utcTime.time_since_epoch().count()) / kSecondsPerDay;
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
}

[[nodiscard]] EphemerisRequest
requestFromContext(const core::SkyContext& context, const IEphemerisEngine& ephemerisEngine) noexcept
{
    EphemerisRequest request;
    request.context = context;
    request.epoch = epochFromUtcTime(context.utcTime);
    request.options = ephemerisEngine.options();
    return request;
}

[[nodiscard]] EphemerisRequest
requestAtUtcTime(const EphemerisRequest& baseRequest, const core::UtcTimePoint& utcTime) noexcept
{
    EphemerisRequest request = baseRequest;
    const double offsetSeconds = std::chrono::duration<double>(utcTime - baseRequest.context.utcTime).count();
    request.context.utcTime = utcTime;
    request.epoch = normalizedAstronomicalEpoch(AstronomicalEpoch{
        .julianDatePart1 = baseRequest.epoch.julianDatePart1,
        .julianDatePart2 = baseRequest.epoch.julianDatePart2 + offsetSeconds / kSecondsPerDay,
        .timeScale = baseRequest.epoch.timeScale,
    });
    return request;
}

[[nodiscard]] std::optional<double> altitudeAt(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& baseRequest,
    const std::uint32_t bodyIndex,
    const core::UtcTimePoint& utcTime
)
{
    const auto request = requestAtUtcTime(baseRequest, utcTime);
    const auto state = ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(bodyIndex));
    if (!state.has_value() || !state->horizontal.isFinite()) {
        return std::nullopt;
    }
    return state->horizontal.altitudeDeg;
}

[[nodiscard]] std::vector<AltitudeSample>
sampleAltitudes(const IEphemerisEngine& ephemerisEngine, const EphemerisRequest& request, const std::uint32_t bodyIndex)
{
    std::vector<AltitudeSample> samples;
    samples.reserve((kSearchHorizonSeconds / kSampleStepSeconds) + 1);

    for (int offsetSeconds = 0; offsetSeconds <= kSearchHorizonSeconds; offsetSeconds += kSampleStepSeconds) {
        const core::UtcTimePoint utcTime = addSeconds(request.context.utcTime, offsetSeconds);
        const auto altitude = altitudeAt(ephemerisEngine, request, bodyIndex, utcTime);
        if (!altitude.has_value()) {
            continue;
        }

        samples.push_back(AltitudeSample{.utcTime = utcTime, .altitudeDeg = *altitude});
    }

    return samples;
}

[[nodiscard]] std::optional<ObservationEventStatus> fixedHorizonStatus(
    const CelestialBody* body, const core::GeoLocation& observer, const double crossingAltitudeDeg
) noexcept
{
    if (body == nullptr || !body->fixedEquatorial.has_value()) {
        return std::nullopt;
    }

    const core::EquatorialCoordinate& equatorial = *body->fixedEquatorial;
    if (!equatorial.isFinite()) {
        return std::nullopt;
    }

    const double maxAltitudeDeg = 90.0 - std::abs(observer.latitudeDeg - equatorial.declinationDeg);
    const double minAltitudeDeg = std::abs(observer.latitudeDeg + equatorial.declinationDeg) - 90.0;

    if (minAltitudeDeg >= crossingAltitudeDeg - kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::AlwaysAbove;
    }
    if (maxAltitudeDeg <= crossingAltitudeDeg + kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::AlwaysBelow;
    }

    return std::nullopt;
}

[[nodiscard]] ObservationEventStatus unavailableCrossingStatus(
    const std::vector<AltitudeSample>& samples, const std::optional<ObservationEventStatus> provenFixedStatus
) noexcept
{
    if (samples.empty()) {
        return ObservationEventStatus::Unresolved;
    }
    if (provenFixedStatus.has_value()) {
        return *provenFixedStatus;
    }

    return ObservationEventStatus::NoEventInSearchWindow;
}

[[nodiscard]] bool
isRiseBracket(const AltitudeSample& previous, const AltitudeSample& next, const double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg < crossingAltitudeDeg && next.altitudeDeg >= crossingAltitudeDeg;
}

[[nodiscard]] bool
isSetBracket(const AltitudeSample& previous, const AltitudeSample& next, const double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg > crossingAltitudeDeg && next.altitudeDeg <= crossingAltitudeDeg;
}

[[nodiscard]] core::UtcTimePoint refinedCrossingTime(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const AltitudeSample& previous,
    const AltitudeSample& next,
    const bool rising,
    const double crossingAltitudeDeg
)
{
    core::UtcTimePoint low = previous.utcTime;
    core::UtcTimePoint high = next.utcTime;

    while ((high - low).count() > kRefinementToleranceSeconds) {
        const auto midpointOffset = (high - low) / 2;
        const core::UtcTimePoint midpoint = low + midpointOffset;
        const auto midpointAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, midpoint);
        if (!midpointAltitude.has_value()) {
            break;
        }

        if (rising) {
            if (*midpointAltitude >= crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        } else {
            if (*midpointAltitude <= crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        }
    }

    return high;
}

[[nodiscard]] ObservationEvent findCrossing(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const std::vector<AltitudeSample>& samples,
    const bool rising,
    const std::optional<ObservationEventStatus> provenFixedStatus,
    const double crossingAltitudeDeg
)
{
    for (std::size_t index = 1; index < samples.size(); ++index) {
        const AltitudeSample& previous = samples[index - 1U];
        const AltitudeSample& next = samples[index];
        const bool bracketed = rising ? isRiseBracket(previous, next, crossingAltitudeDeg)
                                      : isSetBracket(previous, next, crossingAltitudeDeg);
        if (!bracketed) {
            continue;
        }

        return ObservationEvent{
            .status = ObservationEventStatus::Available,
            .utcTime =
                refinedCrossingTime(ephemerisEngine, request, bodyIndex, previous, next, rising, crossingAltitudeDeg)
        };
    }

    return ObservationEvent{.status = unavailableCrossingStatus(samples, provenFixedStatus), .utcTime = std::nullopt};
}

[[nodiscard]] AltitudeSample refinedMaximum(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const core::UtcTimePoint& startUtc,
    const core::UtcTimePoint& endUtc
)
{
    auto lowSeconds = startUtc.time_since_epoch().count();
    auto highSeconds = endUtc.time_since_epoch().count();

    while (highSeconds - lowSeconds > 3) {
        const auto spanSeconds = highSeconds - lowSeconds;
        const auto firstSeconds = lowSeconds + spanSeconds / 3;
        const auto secondSeconds = highSeconds - spanSeconds / 3;
        const core::UtcTimePoint firstUtc{std::chrono::seconds(firstSeconds)};
        const core::UtcTimePoint secondUtc{std::chrono::seconds(secondSeconds)};
        const auto firstAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, firstUtc);
        const auto secondAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, secondUtc);
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
        const core::UtcTimePoint utcTime{std::chrono::seconds(seconds)};
        const auto altitude = altitudeAt(ephemerisEngine, request, bodyIndex, utcTime);
        if (altitude.has_value() && *altitude > best.altitudeDeg) {
            best.utcTime = utcTime;
            best.altitudeDeg = *altitude;
        }
    }

    return best;
}

[[nodiscard]] ObservationCulmination findCulmination(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const std::vector<AltitudeSample>& samples
)
{
    for (std::size_t index = 1; index + 1U < samples.size(); ++index) {
        const AltitudeSample& previous = samples[index - 1U];
        const AltitudeSample& current = samples[index];
        const AltitudeSample& next = samples[index + 1U];

        if (current.altitudeDeg < previous.altitudeDeg || current.altitudeDeg < next.altitudeDeg) {
            continue;
        }

        const AltitudeSample maximum =
            refinedMaximum(ephemerisEngine, request, bodyIndex, previous.utcTime, next.utcTime);
        if (!std::isfinite(maximum.altitudeDeg)) {
            break;
        }

        return ObservationCulmination{
            .status = ObservationEventStatus::Available, .utcTime = maximum.utcTime, .altitudeDeg = maximum.altitudeDeg
        };
    }

    return ObservationCulmination{
        .status = samples.empty() ? ObservationEventStatus::Unresolved : ObservationEventStatus::NoEventInSearchWindow,
        .utcTime = std::nullopt,
        .altitudeDeg = std::nullopt
    };
}

[[nodiscard]] ObservationEventSummary invalidSummary() noexcept
{
    return ObservationEventSummary{
        .nextRise = {.status = ObservationEventStatus::InvalidInput},
        .nextSet = {.status = ObservationEventStatus::InvalidInput},
        .culmination = {.status = ObservationEventStatus::InvalidInput}
    };
}

[[nodiscard]] ObservationEventSummary computeObservationEvents(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const double crossingAltitudeDeg
)
{
    const core::SkyContext& context = request.context;
    if (!context.observer.isValid() || !std::isfinite(crossingAltitudeDeg)) {
        return invalidSummary();
    }

    const auto samples = sampleAltitudes(ephemerisEngine, request, bodyIndex);
    if (samples.empty()) {
        return ObservationEventSummary{};
    }

    const auto provenFixedStatus = fixedHorizonStatus(body, context.observer, crossingAltitudeDeg);
    return ObservationEventSummary{
        .nextRise =
            findCrossing(ephemerisEngine, request, bodyIndex, samples, true, provenFixedStatus, crossingAltitudeDeg),
        .nextSet =
            findCrossing(ephemerisEngine, request, bodyIndex, samples, false, provenFixedStatus, crossingAltitudeDeg),
        .culmination = findCulmination(ephemerisEngine, request, bodyIndex, samples)
    };
}

}  // namespace

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine, const core::SkyContext& context, const std::uint32_t bodyIndex
) const
{
    return compute(ephemerisEngine, requestFromContext(context, ephemerisEngine), bodyIndex, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(
        ephemerisEngine, requestFromContext(context, ephemerisEngine), bodyIndex, nullptr, crossingAltitudeDeg
    );
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const CelestialBody& body
) const
{
    return compute(ephemerisEngine, requestFromContext(context, ephemerisEngine), bodyIndex, body, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const CelestialBody& body,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(
        ephemerisEngine, requestFromContext(context, ephemerisEngine), bodyIndex, &body, crossingAltitudeDeg
    );
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine, const EphemerisRequest& request, const std::uint32_t bodyIndex
) const
{
    return compute(ephemerisEngine, request, bodyIndex, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(ephemerisEngine, request, bodyIndex, nullptr, crossingAltitudeDeg);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody& body
) const
{
    return compute(ephemerisEngine, request, bodyIndex, body, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody& body,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(ephemerisEngine, request, bodyIndex, &body, crossingAltitudeDeg);
}

}  // namespace skygate::ephemeris
