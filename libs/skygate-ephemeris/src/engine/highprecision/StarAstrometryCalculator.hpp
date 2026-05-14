#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

namespace skygate::ephemeris::highprecision {

class StarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    explicit StarAstrometryCalculator(
        std::shared_ptr<const ICalcephKernelProvider> kernelProvider = {},
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService = {}
    );

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;

private:
    std::shared_ptr<const ICalcephKernelProvider> m_kernelProvider;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
};

}  // namespace skygate::ephemeris::highprecision
