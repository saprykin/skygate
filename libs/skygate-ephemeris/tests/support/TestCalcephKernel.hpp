#pragma once

#include "engine/highprecision/ICalcephKernel.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris::tests {

class TestCalcephKernel final : public skygate::ephemeris::highprecision::ICalcephKernel {
public:
    struct Call {
        skygate::core::AstronomicalEpoch epoch;
        int targetNaifId = 0;
        int centerNaifId = 0;
    };

    TestCalcephKernel() = default;
    explicit TestCalcephKernel(Info info);

    [[nodiscard]] Status status() const noexcept override;
    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept override;
    [[nodiscard]] const std::optional<Info>& kernelInfo() const noexcept override;
    [[nodiscard]] Status statusForEpoch(const skygate::core::AstronomicalEpoch& epoch) const noexcept override;
    [[nodiscard]] skygate::ephemeris::highprecision::ICalcephKernel::StateResult
    compute(const skygate::core::AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const override;

    void setStatus(Status status) noexcept;
    void setDiagnostics(std::vector<std::string> diagnostics);
    void setKernelInfo(Info info);
    void clearKernelInfo() noexcept;
    void setRequiredTimeScale(skygate::core::TimeScale timeScale);
    void clearRequiredTimeScale() noexcept;
    void setValidateEpochRange(bool validateEpochRange) noexcept;
    void setDefaultResult(skygate::ephemeris::highprecision::ICalcephKernel::StateResult result);
    [[nodiscard]] skygate::ephemeris::highprecision::ICalcephKernel::StateResult& defaultResult() noexcept;
    [[nodiscard]] skygate::ephemeris::highprecision::ICalcephKernel::StateResult&
    response(int targetNaifId, int centerNaifId);
    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::ICalcephKernel::StateResult>&
    responseSequence(int targetNaifId, int centerNaifId);
    void setResponse(
        int targetNaifId, int centerNaifId, skygate::ephemeris::highprecision::ICalcephKernel::StateResult result
    );
    void appendResponse(
        int targetNaifId, int centerNaifId, skygate::ephemeris::highprecision::ICalcephKernel::StateResult result
    );

    [[nodiscard]] int callCount() const noexcept;
    [[nodiscard]] const skygate::core::AstronomicalEpoch& lastEpoch() const noexcept;
    [[nodiscard]] int lastTargetNaifId() const noexcept;
    [[nodiscard]] int lastCenterNaifId() const noexcept;
    [[nodiscard]] const std::vector<Call>& calls() const noexcept;

private:
    using ResponseKey = std::pair<int, int>;

    [[nodiscard]] ResponseKey responseKey(int targetNaifId, int centerNaifId) const noexcept;

    Status m_status = Status::Ready;
    std::vector<std::string> m_diagnostics;
    std::optional<Info> m_kernelInfo;
    std::optional<skygate::core::TimeScale> m_requiredTimeScale;
    bool m_validateEpochRange = true;
    skygate::ephemeris::highprecision::ICalcephKernel::StateResult m_defaultResult;
    std::map<ResponseKey, skygate::ephemeris::highprecision::ICalcephKernel::StateResult> m_responses;
    std::map<ResponseKey, std::vector<skygate::ephemeris::highprecision::ICalcephKernel::StateResult>>
        m_responseSequences;
    mutable std::map<ResponseKey, std::size_t> m_responseSequenceIndexes;
    mutable int m_callCount = 0;
    mutable skygate::core::AstronomicalEpoch m_lastEpoch;
    mutable int m_lastTargetNaifId = 0;
    mutable int m_lastCenterNaifId = 0;
    mutable std::vector<Call> m_calls;
};

}  // namespace skygate::ephemeris::tests
