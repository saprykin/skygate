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
        AstronomicalEpoch epoch;
        int targetNaifId = 0;
        int centerNaifId = 0;
    };

    TestCalcephKernel() = default;
    explicit TestCalcephKernel(Info info);

    [[nodiscard]] Status status() const noexcept override;
    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept override;
    [[nodiscard]] const std::optional<Info>& kernelInfo() const noexcept override;
    [[nodiscard]] Status statusForEpoch(const AstronomicalEpoch& epoch) const noexcept override;
    [[nodiscard]] skygate::ephemeris::highprecision::SolarSystemKernelStateResult
    compute(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const override;

    void setStatus(Status status) noexcept;
    void setDiagnostics(std::vector<std::string> diagnostics);
    void setKernelInfo(Info info);
    void clearKernelInfo() noexcept;
    void setRequiredTimeScale(TimeScale timeScale);
    void clearRequiredTimeScale() noexcept;
    void setValidateEpochRange(bool validateEpochRange) noexcept;
    void setDefaultResult(skygate::ephemeris::highprecision::SolarSystemKernelStateResult result);
    [[nodiscard]] skygate::ephemeris::highprecision::SolarSystemKernelStateResult& defaultResult() noexcept;
    [[nodiscard]] skygate::ephemeris::highprecision::SolarSystemKernelStateResult&
    response(int targetNaifId, int centerNaifId);
    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::SolarSystemKernelStateResult>&
    responseSequence(int targetNaifId, int centerNaifId);
    void setResponse(
        int targetNaifId, int centerNaifId, skygate::ephemeris::highprecision::SolarSystemKernelStateResult result
    );
    void appendResponse(
        int targetNaifId, int centerNaifId, skygate::ephemeris::highprecision::SolarSystemKernelStateResult result
    );

    [[nodiscard]] int callCount() const noexcept;
    [[nodiscard]] const AstronomicalEpoch& lastEpoch() const noexcept;
    [[nodiscard]] int lastTargetNaifId() const noexcept;
    [[nodiscard]] int lastCenterNaifId() const noexcept;
    [[nodiscard]] const std::vector<Call>& calls() const noexcept;

private:
    using ResponseKey = std::pair<int, int>;

    [[nodiscard]] ResponseKey responseKey(int targetNaifId, int centerNaifId) const noexcept;

    Status m_status = Status::Ready;
    std::vector<std::string> m_diagnostics;
    std::optional<Info> m_kernelInfo;
    std::optional<TimeScale> m_requiredTimeScale;
    bool m_validateEpochRange = true;
    skygate::ephemeris::highprecision::SolarSystemKernelStateResult m_defaultResult;
    std::map<ResponseKey, skygate::ephemeris::highprecision::SolarSystemKernelStateResult> m_responses;
    std::map<ResponseKey, std::vector<skygate::ephemeris::highprecision::SolarSystemKernelStateResult>>
        m_responseSequences;
    mutable std::map<ResponseKey, std::size_t> m_responseSequenceIndexes;
    mutable int m_callCount = 0;
    mutable AstronomicalEpoch m_lastEpoch;
    mutable int m_lastTargetNaifId = 0;
    mutable int m_lastCenterNaifId = 0;
    mutable std::vector<Call> m_calls;
};

}  // namespace skygate::ephemeris::tests
