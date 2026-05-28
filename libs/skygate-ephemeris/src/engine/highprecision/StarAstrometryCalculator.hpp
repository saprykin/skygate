#pragma once

#include "CatalogStarAstrometryArrays.hpp"
#include "IStarAstrometryCalculator.hpp"

#include <cstddef>
#include <vector>

namespace skygate::ephemeris::highprecision {

class StarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    explicit StarAstrometryCalculator(
        std::shared_ptr<const ICalcephKernelProvider> kernelProvider = {},
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService = {}
    );

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;
    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override;

private:
    std::shared_ptr<const ICalcephKernelProvider> m_kernelProvider;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
};

}  // namespace skygate::ephemeris::highprecision
