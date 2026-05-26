#pragma once

#include "ObservationEvent.hpp"
#include "ObservationEventSummary.hpp"

#include <cstdint>

namespace skygate::ephemeris {

struct CelestialBody;
struct EphemerisRequest;
class IEphemerisEngine;

class ObservationEventCalculator final {
public:
    enum class SearchMode : std::uint8_t {
        Guided,
        GuidedApproximate,
        Direct
    };

    [[nodiscard]] ObservationEventSummary compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const CelestialBody* body,
        double crossingAltitudeDeg,
        SearchMode searchMode
    ) const;
};

}  // namespace skygate::ephemeris
