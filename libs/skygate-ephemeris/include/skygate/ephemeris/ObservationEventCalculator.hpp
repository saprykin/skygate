#pragma once

#include "skygate/core/Types.hpp"

#include <cstdint>
#include <optional>

namespace skygate::ephemeris {

struct CelestialBody;
struct EphemerisRequest;
class IEphemerisEngine;

enum class ObservationEventStatus : std::uint8_t {
    Available,
    AlwaysAbove,
    AlwaysBelow,
    NoEventInSearchWindow,
    InvalidInput,
    Unresolved
};

struct ObservationEvent {
    ObservationEventStatus status = ObservationEventStatus::Unresolved;
    std::optional<core::UtcTimePoint> utcTime;
};

struct ObservationCulmination {
    ObservationEventStatus status = ObservationEventStatus::Unresolved;
    std::optional<core::UtcTimePoint> utcTime;
    std::optional<double> altitudeDeg;
};

struct ObservationEventSummary {
    ObservationEvent nextRise;
    ObservationEvent nextSet;
    ObservationCulmination culmination;
};

enum class ObservationEventSearchMode : std::uint8_t {
    Guided,
    GuidedApproximate,
    Direct
};

class ObservationEventCalculator final {
public:
    [[nodiscard]] ObservationEventSummary
    compute(const IEphemerisEngine& ephemerisEngine, const core::SkyContext& context, std::uint32_t bodyIndex) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const core::SkyContext& context,
        std::uint32_t bodyIndex,
        double crossingAltitudeDeg
    ) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const core::SkyContext& context,
        std::uint32_t bodyIndex,
        const CelestialBody& body
    ) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const core::SkyContext& context,
        std::uint32_t bodyIndex,
        const CelestialBody& body,
        double crossingAltitudeDeg
    ) const;
    [[nodiscard]] ObservationEventSummary
    compute(const IEphemerisEngine& ephemerisEngine, const EphemerisRequest& request, std::uint32_t bodyIndex) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        double crossingAltitudeDeg
    ) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const CelestialBody& body
    ) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const CelestialBody& body,
        double crossingAltitudeDeg
    ) const;
    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const CelestialBody& body,
        double crossingAltitudeDeg,
        ObservationEventSearchMode searchMode
    ) const;
};

}  // namespace skygate::ephemeris
