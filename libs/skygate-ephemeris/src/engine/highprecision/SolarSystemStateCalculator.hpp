#pragma once

#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "ISolarSystemStateCalculator.hpp"

#include <memory>

namespace skygate::ephemeris::highprecision {

class ICalcephKernel;

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
