#include "StaticCalcephKernelProvider.hpp"

#include <utility>

namespace skygate::ephemeris::tests {

StaticCalcephKernelProvider::StaticCalcephKernelProvider(
    std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel
)
    : m_kernel(std::move(kernel))
{
}

std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> StaticCalcephKernelProvider::openKernel() const
{
    ++m_openCount;
    return m_kernel;
}

void StaticCalcephKernelProvider::setKernel(
    std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel
)
{
    m_kernel = std::move(kernel);
}

int StaticCalcephKernelProvider::openCount() const noexcept
{
    return m_openCount;
}

}  // namespace skygate::ephemeris::tests
