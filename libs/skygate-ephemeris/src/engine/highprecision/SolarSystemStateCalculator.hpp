#pragma once

#include "ISolarSystemStateCalculator.hpp"

#include <memory>

namespace skygate::ephemeris::highprecision {

class SolarSystemStateCalculator final : public ISolarSystemStateCalculator {
public:
    explicit SolarSystemStateCalculator(
        std::shared_ptr<const ICalcephKernelProvider> kernelProvider, bool preferPlanetarySystemBarycenters = false
    );

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;

private:
    std::shared_ptr<const ICalcephKernelProvider> m_kernelProvider;
    bool m_preferPlanetarySystemBarycenters = false;
};

}  // namespace skygate::ephemeris::highprecision
