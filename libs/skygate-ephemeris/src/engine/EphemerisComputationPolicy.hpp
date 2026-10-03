#pragma once

#include "EphemerisPrecisionPolicy.hpp"
#include "EphemerisRequest.hpp"
#include "ObservationEventCalculator.hpp"

namespace skygate::ephemeris {

class BaseCelestialBody;
class IEphemerisEngine;

class EphemerisComputationPolicy final {
public:
    EphemerisComputationPolicy() = delete;

    [[nodiscard]] static EphemerisRequest correctionFlagsFor(
        const IEphemerisEngine& engine, EphemerisRequest request, EphemerisPrecisionPolicy policy
    ) noexcept;

    [[nodiscard]] static bool
    shouldRecomputeInspectorDetail(const IEphemerisEngine& engine, const EphemerisRequest& request) noexcept;

    [[nodiscard]] static ObservationEventCalculator::SearchMode inspectorEventSearchMode(
        const IEphemerisEngine& engine, const EphemerisRequest& request, const BaseCelestialBody* body
    ) noexcept;

    [[nodiscard]] static bool
    trailSamplingIsAdaptive(const IEphemerisEngine& engine, const EphemerisRequest& request) noexcept;

    // True when adaptive sampling is selected and the engine has granted
    // permission to approximate the trail through a guidance strategy or the
    // fixed-equatorial shortcut. body == nullptr always returns false.
    [[nodiscard]] static bool trailUsesGuidance(
        const IEphemerisEngine& engine, const EphemerisRequest& request, const BaseCelestialBody* body
    ) noexcept;

    [[nodiscard]] static bool trailAllowsGuidanceApproximation(const IEphemerisEngine& engine) noexcept;

    [[nodiscard]] static bool liveRecomputeThrottleApplies(const IEphemerisEngine& engine) noexcept;
};

}  // namespace skygate::ephemeris
