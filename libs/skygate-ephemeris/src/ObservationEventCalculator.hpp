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
    /// Constructs a calculator with no guidance strategy. `compute` falls back
    /// to direct sampling whenever guidance is unavailable.
    ObservationEventCalculator();

    /// Constructs a calculator with the supplied guidance strategy. Passing
    /// nullptr is equivalent to no guidance and also falls back to direct
    /// sampling.
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
