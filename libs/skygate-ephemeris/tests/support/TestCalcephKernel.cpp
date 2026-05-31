#include "TestCalcephKernel.hpp"

#include <cstddef>
#include <utility>

namespace skygate::ephemeris::tests {

TestCalcephKernel::TestCalcephKernel(Info info) : m_kernelInfo(std::move(info)) {}

TestCalcephKernel::Status TestCalcephKernel::status() const noexcept
{
    return m_status;
}

const std::vector<std::string>& TestCalcephKernel::diagnostics() const noexcept
{
    return m_diagnostics;
}

const std::optional<TestCalcephKernel::Info>& TestCalcephKernel::kernelInfo() const noexcept
{
    return m_kernelInfo;
}

TestCalcephKernel::Status TestCalcephKernel::statusForEpoch(const AstronomicalEpoch& epoch) const noexcept
{
    if (m_status != Status::Ready || !m_validateEpochRange || !m_kernelInfo.has_value()) {
        return m_status;
    }
    if (!epoch.isFinite() || epoch.sortKey() < m_kernelInfo->validityRange.start.sortKey()
        || epoch.sortKey() > m_kernelInfo->validityRange.end.sortKey()) {
        return Status::OutOfRange;
    }

    return Status::Ready;
}

skygate::ephemeris::highprecision::SolarSystemKernelStateResult
TestCalcephKernel::compute(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const
{
    ++m_callCount;
    m_lastEpoch = epoch;
    m_lastTargetNaifId = targetNaifId;
    m_lastCenterNaifId = centerNaifId;
    m_calls.push_back({
        .epoch = epoch,
        .targetNaifId = targetNaifId,
        .centerNaifId = centerNaifId,
    });
    if (m_requiredTimeScale.has_value() && epoch.timeScale != *m_requiredTimeScale) {
        skygate::ephemeris::highprecision::SolarSystemKernelStateResult result;
        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return result;
    }

    const ResponseKey key = responseKey(targetNaifId, centerNaifId);
    if (const auto sequence = m_responseSequences.find(key); sequence != m_responseSequences.end()) {
        const std::size_t index = m_responseSequenceIndexes[key]++;
        if (index < sequence->second.size()) {
            return sequence->second[index];
        }
        return sequence->second.back();
    }
    if (const auto match = m_responses.find(key); match != m_responses.end()) {
        return match->second;
    }
    return m_defaultResult;
}

void TestCalcephKernel::setStatus(const Status status) noexcept
{
    m_status = status;
}

void TestCalcephKernel::setDiagnostics(std::vector<std::string> diagnostics)
{
    m_diagnostics = std::move(diagnostics);
}

void TestCalcephKernel::setKernelInfo(Info info)
{
    m_kernelInfo = std::move(info);
}

void TestCalcephKernel::clearKernelInfo() noexcept
{
    m_kernelInfo = std::nullopt;
}

void TestCalcephKernel::setRequiredTimeScale(const TimeScale timeScale)
{
    m_requiredTimeScale = timeScale;
}

void TestCalcephKernel::clearRequiredTimeScale() noexcept
{
    m_requiredTimeScale = std::nullopt;
}

void TestCalcephKernel::setValidateEpochRange(const bool validateEpochRange) noexcept
{
    m_validateEpochRange = validateEpochRange;
}

void TestCalcephKernel::setDefaultResult(skygate::ephemeris::highprecision::SolarSystemKernelStateResult result)
{
    m_defaultResult = std::move(result);
}

skygate::ephemeris::highprecision::SolarSystemKernelStateResult& TestCalcephKernel::defaultResult() noexcept
{
    return m_defaultResult;
}

skygate::ephemeris::highprecision::SolarSystemKernelStateResult&
TestCalcephKernel::response(const int targetNaifId, const int centerNaifId)
{
    return m_responses[responseKey(targetNaifId, centerNaifId)];
}

std::vector<skygate::ephemeris::highprecision::SolarSystemKernelStateResult>&
TestCalcephKernel::responseSequence(const int targetNaifId, const int centerNaifId)
{
    return m_responseSequences[responseKey(targetNaifId, centerNaifId)];
}

void TestCalcephKernel::setResponse(
    const int targetNaifId,
    const int centerNaifId,
    skygate::ephemeris::highprecision::SolarSystemKernelStateResult result
)
{
    m_responses[responseKey(targetNaifId, centerNaifId)] = std::move(result);
}

void TestCalcephKernel::appendResponse(
    const int targetNaifId,
    const int centerNaifId,
    skygate::ephemeris::highprecision::SolarSystemKernelStateResult result
)
{
    m_responseSequences[responseKey(targetNaifId, centerNaifId)].push_back(std::move(result));
}

int TestCalcephKernel::callCount() const noexcept
{
    return m_callCount;
}

const AstronomicalEpoch& TestCalcephKernel::lastEpoch() const noexcept
{
    return m_lastEpoch;
}

int TestCalcephKernel::lastTargetNaifId() const noexcept
{
    return m_lastTargetNaifId;
}

int TestCalcephKernel::lastCenterNaifId() const noexcept
{
    return m_lastCenterNaifId;
}

const std::vector<TestCalcephKernel::Call>& TestCalcephKernel::calls() const noexcept
{
    return m_calls;
}

TestCalcephKernel::ResponseKey
TestCalcephKernel::responseKey(const int targetNaifId, const int centerNaifId) const noexcept
{
    return {targetNaifId, centerNaifId};
}

}  // namespace skygate::ephemeris::tests
