#pragma once

#include "engine/IEphemerisGuidanceStrategy.hpp"
#include "NightConditions.hpp"

#include <cstdint>
#include <memory>

namespace skygate::ephemeris {

class BaseCelestialBody;
struct EphemerisRequest;
class IEphemerisEngine;

class NightConditionsCalculator final {
public:
    /// Constructs a calculator with no guidance strategy. `compute` falls back
    /// to direct sampling whenever guidance is unavailable.
    NightConditionsCalculator();

    /// Constructs a calculator with the supplied guidance strategy. Passing
    /// nullptr is equivalent to no guidance and also falls back to direct
    /// sampling.
    explicit NightConditionsCalculator(std::shared_ptr<IEphemerisGuidanceStrategy> guidanceStrategy);

    /// Event search mode. Approximate mode events may still be refined or
    /// verified against the primary engine when the engine does not grant
    /// `trustsGuidedSearchResult`.
    enum class EventSearchMode : std::uint8_t {
        Approximate,
        Verified
    };

    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t sunBodyIndex,
        const BaseCelestialBody* sunBody,
        std::uint32_t moonBodyIndex,
        const BaseCelestialBody* moonBody,
        EventSearchMode eventSearchMode = EventSearchMode::Approximate
    ) const;

private:
    std::shared_ptr<IEphemerisGuidanceStrategy> m_guidanceStrategy;
};

}  // namespace skygate::ephemeris
