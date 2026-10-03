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
    NightConditionsCalculator();
    explicit NightConditionsCalculator(std::shared_ptr<IEphemerisGuidanceStrategy> guidanceStrategy);

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
