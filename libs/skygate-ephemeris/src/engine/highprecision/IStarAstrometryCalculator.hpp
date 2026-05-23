#pragma once

#include "engine/highprecision/CatalogStarAstrometryArrays.hpp"
#include "engine/highprecision/HighPrecisionTypes.hpp"

#include <cstddef>
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
    ) const;
};

}  // namespace skygate::ephemeris::highprecision
