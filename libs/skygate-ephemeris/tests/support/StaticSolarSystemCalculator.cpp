#include "StaticSolarSystemCalculator.hpp"

#include <utility>

namespace skygate::ephemeris::tests {

using skygate::ephemeris::highprecision::HighPrecisionCalculatorResult;
using skygate::ephemeris::highprecision::HighPrecisionComputationInput;

StaticSolarSystemCalculator::StaticSolarSystemCalculator(HighPrecisionCalculatorResult result)
    : m_result(std::move(result))
{
}

HighPrecisionCalculatorResult StaticSolarSystemCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    ++m_callCount;
    m_lastBodyId = input.body.id;
    return m_result;
}

int StaticSolarSystemCalculator::callCount() const noexcept
{
    return m_callCount;
}

const std::string& StaticSolarSystemCalculator::lastBodyId() const noexcept
{
    return m_lastBodyId;
}

}  // namespace skygate::ephemeris::tests
