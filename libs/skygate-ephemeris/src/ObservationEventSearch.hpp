#pragma once

#include "ObservationEvent.hpp"
#include "ObservationEventStatus.hpp"
#include "ObservationEventSummary.hpp"
#include "UtcTimePoint.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace skygate::ephemeris {

class BaseCelestialBody;
class IEphemerisEngine;
struct EphemerisRequest;

// Altitude sampling and refinement for one crossing altitude.
//
// The search keeps references to the engine and request it was built from, so
// both must outlive it. It never copies the request: every ephemeris call
// derives a fresh request from the stored one and leaves ownership with the
// caller.
class ObservationEventSearch final {
public:
    struct AltitudeSample final {
        skygate::core::UtcTimePoint utcTime;
        double altitudeDeg = std::numeric_limits<double>::quiet_NaN();
    };

    // Origin of the samples in one search pass. Guidance samples are
    // approximate and may be verified against the primary engine.
    enum class SampleRole : std::uint8_t {
        Direct,
        Guidance
    };

    ObservationEventSearch(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const BaseCelestialBody* body,
        double crossingAltitudeDeg
    ) noexcept;

    // Samples the supplied engine and request instead of the ones this search
    // was built from. Guidance sampling passes its own engine, request, and
    // body index.
    [[nodiscard]] std::vector<AltitudeSample>
    sampleAltitudes(const IEphemerisEngine& engine, const EphemerisRequest& request, std::uint32_t bodyIndex);

    [[nodiscard]] ObservationEvent findCrossing(
        const std::vector<AltitudeSample>& samples,
        SampleRole sampleRole,
        bool trustGuidance,
        bool rising,
        std::optional<ObservationEventStatus::Type> provenFixedStatus
    );

    [[nodiscard]] ObservationEvent findCulmination(const std::vector<AltitudeSample>& samples, bool trustGuidance);

    // Diurnal range classification of a fixed body against the crossing
    // altitude. Returns nullopt when the body is not fixed, its catalog
    // coordinate is not finite, or the crossing altitude lies inside the range
    // so a crossing stays possible.
    [[nodiscard]] std::optional<ObservationEventStatus::Type> fixedHorizonStatus() const noexcept;

    [[nodiscard]] static ObservationEventSummary invalidSummary() noexcept
    {
        return ObservationEventSummary{
            .nextRise = {.status = ObservationEventStatus::Type::InvalidInput},
            .nextSet = {.status = ObservationEventStatus::Type::InvalidInput},
            .culmination = {.status = ObservationEventStatus::Type::InvalidInput}
        };
    }

    // Longest search window used when a crossing event is not found earlier.
    static constexpr int kSearchHorizonHours = 72;

private:
    [[nodiscard]] std::optional<double> altitudeAt(const skygate::core::UtcTimePoint& utcTime);

    [[nodiscard]] std::optional<std::array<AltitudeSample, 2>>
    originalAltitudeBracket(const AltitudeSample& previous, const AltitudeSample& next);

    [[nodiscard]] skygate::core::UtcTimePoint refinedCrossingTime(
        const AltitudeSample& previous,
        const AltitudeSample& next,
        bool rising,
        skygate::core::UtcTimePoint::duration refinementTolerance
    );

    [[nodiscard]] AltitudeSample
    refinedMaximum(const skygate::core::UtcTimePoint& startUtc, const skygate::core::UtcTimePoint& endUtc);

    const IEphemerisEngine& m_ephemerisEngine;
    const EphemerisRequest& m_request;
    std::uint32_t m_bodyIndex;
    const BaseCelestialBody* m_body;
    double m_crossingAltitudeDeg;
};

}  // namespace skygate::ephemeris
