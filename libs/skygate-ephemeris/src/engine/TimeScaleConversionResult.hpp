#pragma once

#include "TimeScaleConversionDiagnostics.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstdint>
#include <string>

namespace skygate::ephemeris {

struct TimeScaleConversionResult {
    skygate::core::AstronomicalEpoch epoch;
    TimeScaleConversionStatus status = TimeScaleConversionStatus::Failed;
    std::uint32_t warningCodeMask = 0U;
    std::string diagnosticText;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == TimeScaleConversionStatus::Valid || status == TimeScaleConversionStatus::Degraded;
    }

    void addWarning(const TimeScaleConversionWarningCode code) noexcept
    {
        warningCodeMask |= TimeScaleConversionDiagnostics::warningMask(code);
    }

    [[nodiscard]] bool hasWarning(const TimeScaleConversionWarningCode code) const noexcept
    {
        return (warningCodeMask & TimeScaleConversionDiagnostics::warningMask(code)) != 0U;
    }
};

}  // namespace skygate::ephemeris
