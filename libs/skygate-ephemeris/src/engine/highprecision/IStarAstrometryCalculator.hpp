#pragma once

#include "CatalogStarAstrometryArrays.hpp"
#include "HighPrecisionTypes.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace skygate::ephemeris::highprecision {

class IStarAstrometryCalculator {
public:
    virtual ~IStarAstrometryCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const = 0;

    [[nodiscard]] virtual std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
