#pragma once

#include "BaseCelestialBody.hpp"
#include "EphemerisRequest.hpp"
#include "ObservationEvent.hpp"
#include "ObservationEventCalculator.hpp"
#include "ObservationEventStatus.hpp"
#include "engine/IEphemerisEngine.hpp"

namespace skygate::ephemeris {

// Decisions that turn engine traits, request options, and body geometry into
// guidance and fallback behavior for an observation event search.
class ObservationEventSearchPolicy final {
public:
    ObservationEventSearchPolicy() = delete;

    // Guidance sampling needs a mode that asks for it, a body to build the
    // guidance catalog from, and an engine that advertises guided event search.
    [[nodiscard]] static bool shouldUseGuidanceEngine(
        const IEphemerisEngine& engine, const BaseCelestialBody* body, ObservationEventCalculator::SearchMode searchMode
    ) noexcept
    {
        return (searchMode == ObservationEventCalculator::SearchMode::Guided
                || searchMode == ObservationEventCalculator::SearchMode::GuidedApproximate)
               && body != nullptr && engine.traits().supportsGuidedEventSearch;
    }

    // A guided answer may be reported directly only when the engine trusts it
    // and either the mode asks for an approximation or a corrected fixed body
    // makes the primary engine redundant.
    [[nodiscard]] static bool isTrustingGuidanceModel(
        const IEphemerisEngine& engine,
        const BaseCelestialBody* body,
        const EphemerisRequest& request,
        ObservationEventCalculator::SearchMode searchMode
    ) noexcept
    {
        if (!engine.traits().trustsGuidedSearchResult) {
            return false;
        }

        return searchMode == ObservationEventCalculator::SearchMode::GuidedApproximate
               || (body != nullptr && body->hasFixedEquatorial() && request.options.correctionFlags().hasCorrections());
    }

    // Direct sampling is the verification mechanism for every untrusted
    // no-crossing conclusion. A missed crossing, an always-above/below
    // classification, or a window miss must be rechecked against the primary
    // engine before it is reported, including for fixed bodies whose catalog
    // geometry may not match the engine's corrected positions.
    [[nodiscard]] static bool shouldFallBackToDirect(
        ObservationEventCalculator::SearchMode searchMode,
        const BaseCelestialBody* body,
        bool trustGuidance,
        const ObservationEvent& nextRise,
        const ObservationEvent& nextSet
    ) noexcept
    {
        if (!trustGuidance) {
            return nextRise.status != ObservationEventStatus::Type::Available
                   || nextSet.status != ObservationEventStatus::Type::Available;
        }

        if (searchMode == ObservationEventCalculator::SearchMode::GuidedApproximate) {
            return false;
        }

        const bool fixedBody = body != nullptr && body->hasFixedEquatorial();
        const bool unresolved = nextRise.status == ObservationEventStatus::Type::Unresolved
                                || nextSet.status == ObservationEventStatus::Type::Unresolved;
        const bool nonFixedMiss = !fixedBody
                                  && (nextRise.status == ObservationEventStatus::Type::NoEventInSearchWindow
                                      || nextSet.status == ObservationEventStatus::Type::NoEventInSearchWindow);
        return unresolved || nonFixedMiss;
    }
};

}  // namespace skygate::ephemeris
