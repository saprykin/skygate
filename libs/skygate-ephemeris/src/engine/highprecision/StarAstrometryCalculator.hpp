#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

namespace skygate::ephemeris::highprecision {

class StarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    explicit StarAstrometryCalculator(std::shared_ptr<const ICalcephKernelProvider> kernelProvider = {});

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;

private:
    std::shared_ptr<const ICalcephKernelProvider> m_kernelProvider;
};

}  // namespace skygate::ephemeris::highprecision
