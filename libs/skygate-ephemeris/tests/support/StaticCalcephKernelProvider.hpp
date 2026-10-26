#pragma once

#include "engine/highprecision/ICalcephKernelProvider.hpp"

#include <memory>

namespace skygate::ephemeris::tests {

class StaticCalcephKernelProvider final : public skygate::ephemeris::highprecision::ICalcephKernelProvider {
public:
    StaticCalcephKernelProvider() = default;
    explicit StaticCalcephKernelProvider(
        std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel
    );

    [[nodiscard]] std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> openKernel() const override;

    void setKernel(std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel);
    [[nodiscard]] int openCount() const noexcept;

private:
    std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> m_kernel;
    mutable int m_openCount = 0;
};

}  // namespace skygate::ephemeris::tests
