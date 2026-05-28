#pragma once

#include "ObservationEventSummary.hpp"

#include <cstdint>

namespace skygate::ephemeris {

class BaseCelestialBody;
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
        const BaseCelestialBody* body,
        double crossingAltitudeDeg,
        SearchMode searchMode
    ) const;
};

}  // namespace skygate::ephemeris
