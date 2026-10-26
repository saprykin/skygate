#pragma once

#include <memory>

namespace skygate::ephemeris::highprecision {

class ICalcephKernel;

class ICalcephKernelProvider {
public:
    virtual ~ICalcephKernelProvider() = default;

    [[nodiscard]] virtual std::shared_ptr<const ICalcephKernel> openKernel() const = 0;
};

}  // namespace skygate::ephemeris::highprecision
