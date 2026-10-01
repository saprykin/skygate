#pragma once
#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "StarAstrometryBatchResult.hpp"

#include <memory>
#include <span>
#include <vector>

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

class IApparentPlaceCalculator {
public:
    virtual ~IApparentPlaceCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult) const = 0;

    [[nodiscard]] virtual std::vector<StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> bodies,
        std::span<const StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
