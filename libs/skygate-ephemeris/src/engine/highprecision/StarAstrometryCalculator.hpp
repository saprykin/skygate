#pragma once

#include "engine/highprecision/CatalogStarAstrometryArrays.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include <cstddef>
#include <vector>

namespace skygate::ephemeris::highprecision {

struct StarAstrometryBatchResult {
    std::size_t bodyIndex = 0U;
    HighPrecisionCalculatorResult result;
};

class StarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    explicit StarAstrometryCalculator(
        std::shared_ptr<const ICalcephKernelProvider> kernelProvider = {},
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService = {}
    );

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;
    [[nodiscard]] std::vector<StarAstrometryBatchResult>
    calculateBatch(const EphemerisRequest& request, const CatalogStarAstrometryArrays& arrays) const;

private:
    std::shared_ptr<const ICalcephKernelProvider> m_kernelProvider;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
};

}  // namespace skygate::ephemeris::highprecision
