#pragma once

#include "FrameTransformBatcher.hpp"
#include "IAtmosphericRefractionCalculator.hpp"
#include "StarAstrometryBatchResult.hpp"

#include <memory>
#include <span>
#include <vector>

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

// Turns the vectors the frame-transform pass leaves on the batch into batch
// results and applies atmospheric refraction behind the same gate the request
// uses for it.
class ApparentPlaceOutputFinalizer final {
public:
    explicit ApparentPlaceOutputFinalizer(const IAtmosphericRefractionCalculator* atmosphericRefractionCalculator);

    [[nodiscard]] std::vector<StarAstrometryBatchResult> finalize(
        const EphemerisRequest& request,
        const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState,
        std::span<FrameTransformBatcher::Item> items
    ) const;

private:
    [[nodiscard]] static bool finalizeEquatorialCoordinate(FrameTransformBatcher::Item& item);

    void applyAtmosphericRefraction(
        FrameTransformBatcher::Item& item,
        bool canApplyRefraction,
        const EphemerisRequest& request,
        const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState
    ) const;

    const IAtmosphericRefractionCalculator* const m_atmosphericRefractionCalculator;
};

}  // namespace skygate::ephemeris::highprecision
