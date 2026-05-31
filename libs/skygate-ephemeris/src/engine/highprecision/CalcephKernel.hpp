#pragma once

#include "ICalcephKernel.hpp"

#include <memory>
#include <optional>

namespace skygate::ephemeris::highprecision {

class CalcephKernel final : public ICalcephKernel {
public:
    explicit CalcephKernel(Info info);
    ~CalcephKernel() override;

    CalcephKernel(const CalcephKernel&) = delete;
    CalcephKernel& operator=(const CalcephKernel&) = delete;
    CalcephKernel(CalcephKernel&& other) noexcept = delete;
    CalcephKernel& operator=(CalcephKernel&& other) noexcept = delete;

    [[nodiscard]] Status status() const noexcept override;
    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept override;
    [[nodiscard]] const std::optional<Info>& kernelInfo() const noexcept override;
    [[nodiscard]] Status statusForEpoch(const AstronomicalEpoch& epoch) const noexcept override;
    [[nodiscard]] SolarSystemKernelStateResult
    compute(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const override;

private:
    class Impl;

    std::unique_ptr<Impl> m_impl;
};

}  // namespace skygate::ephemeris::highprecision
