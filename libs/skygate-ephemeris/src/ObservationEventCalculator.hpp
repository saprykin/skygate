#pragma once

#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "ObservationEventSummary.hpp"

#include <cstdint>
#include <memory>

namespace skygate::ephemeris {

class BaseCelestialBody;
struct EphemerisRequest;
class IEphemerisEngine;

class ObservationEventCalculator final {
public:
    ObservationEventCalculator();
    explicit ObservationEventCalculator(std::shared_ptr<IEphemerisGuidanceStrategy> guidanceStrategy);

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

private:
    std::shared_ptr<IEphemerisGuidanceStrategy> m_guidanceStrategy;
};

}  // namespace skygate::ephemeris
