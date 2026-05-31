#pragma once

#include "ISolarSystemStateCalculator.hpp"

#include <memory>

namespace skygate::ephemeris::highprecision {

class SolarSystemStateCalculator final : public ISolarSystemStateCalculator {
public:
    explicit SolarSystemStateCalculator(
        std::shared_ptr<const ICalcephKernel> kernel, bool preferPlanetarySystemBarycenters = false
    );

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;

private:
    std::shared_ptr<const ICalcephKernel> m_kernel;
    bool m_preferPlanetarySystemBarycenters = false;
};

}  // namespace skygate::ephemeris::highprecision
