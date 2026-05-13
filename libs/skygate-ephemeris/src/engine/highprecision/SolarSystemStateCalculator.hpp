#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include <memory>

namespace skygate::ephemeris::highprecision {

class SolarSystemStateCalculator final : public ISolarSystemStateCalculator {
public:
    explicit SolarSystemStateCalculator(std::shared_ptr<const ICalcephKernelProvider> kernelProvider);

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;

private:
    std::shared_ptr<const ICalcephKernelProvider> m_kernelProvider;
};

}  // namespace skygate::ephemeris::highprecision
