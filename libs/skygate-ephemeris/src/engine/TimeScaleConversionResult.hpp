#pragma once

#include "BitFlagSetOperations.hpp"
#include "TimeScaleConversionDiagnostics.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstdint>
#include <string>

namespace skygate::ephemeris {

struct TimeScaleConversionResult {
    skygate::core::AstronomicalEpoch epoch;
    TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Failed;
    std::uint32_t warningCodeMask = 0U;
    std::string diagnosticText;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == TimeScaleConversionStatus::Type::Valid || status == TimeScaleConversionStatus::Type::Degraded;
    }

    [[nodiscard]] static TimeScaleConversionResult success(
        skygate::core::AstronomicalEpoch epoch,
        TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Valid,
        std::uint32_t warningCodeMask = 0U,
        std::string diagnosticText = {}
    );

    [[nodiscard]] static TimeScaleConversionResult failure(
        const skygate::core::AstronomicalEpoch& epoch, skygate::core::TimeScale targetScale, std::string diagnosticText
    );

    static void mergeWarnings(
        TimeScaleConversionResult& result,
        TimeScaleConversionStatus::Type sourceStatus,
        std::uint32_t sourceWarningCodeMask
    ) noexcept;

    void addWarning(const TimeScaleConversionWarningCode::Type code) noexcept
    {
        warningCodeMask |= BitFlagSetOperations::bit(code);
    }

    [[nodiscard]] bool hasWarning(const TimeScaleConversionWarningCode::Type code) const noexcept
    {
        return (warningCodeMask & BitFlagSetOperations::bit(code)) != 0U;
    }
};

}  // namespace skygate::ephemeris
