#pragma once

#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"

#include <string>

namespace skygate::ephemeris::tests {

// Serves one canned calculator result, counting calls and remembering the
// body id of the last request so batching expectations stay assertable.
class StaticSolarSystemCalculator final : public skygate::ephemeris::highprecision::ISolarSystemStateCalculator {
public:
    explicit StaticSolarSystemCalculator(skygate::ephemeris::highprecision::HighPrecisionCalculatorResult result);

    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult
    calculate(const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input) const override;

    [[nodiscard]] int callCount() const noexcept;
    [[nodiscard]] const std::string& lastBodyId() const noexcept;

private:
    skygate::ephemeris::highprecision::HighPrecisionCalculatorResult m_result;
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
};

}  // namespace skygate::ephemeris::tests
