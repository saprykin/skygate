#pragma once

#include "IApparentPlaceCalculator.hpp"

#include <memory>
#include <span>
#include <vector>

namespace skygate::ephemeris::highprecision {

class BaseApparentPlaceCalculator : public IApparentPlaceCalculator {
public:
    [[nodiscard]] std::vector<StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> bodies,
        std::span<const StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override;
};

}  // namespace skygate::ephemeris::highprecision
