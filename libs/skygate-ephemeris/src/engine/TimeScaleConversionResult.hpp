#pragma once

#include "BitFlagSet.hpp"
#include "TimeScaleConversionStatus.hpp"
#include "TimeScaleConversionWarningCode.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <string>

namespace skygate::ephemeris {

struct TimeScaleConversionResult {
    using WarningMask = BitFlagSet<TimeScaleConversionWarningCode::Type>;

    skygate::core::AstronomicalEpoch epoch;
    TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Failed;
    WarningMask warningCodeMask;
    std::string diagnosticText;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == TimeScaleConversionStatus::Type::Valid || status == TimeScaleConversionStatus::Type::Degraded;
    }

    [[nodiscard]] static TimeScaleConversionResult success(
        skygate::core::AstronomicalEpoch epoch,
        TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Valid,
        WarningMask warningCodeMask = {},
        std::string diagnosticText = {}
    );

    [[nodiscard]] static TimeScaleConversionResult failure(
        const skygate::core::AstronomicalEpoch& epoch, skygate::core::TimeScale targetScale, std::string diagnosticText
    );

    [[nodiscard]] static TimeScaleConversionResult failureFromLookup(
        const skygate::core::AstronomicalEpoch& epoch,
        skygate::core::TimeScale targetScale,
        WarningMask warningCodeMask,
        std::string diagnosticText
    );

    static void mergeWarnings(
        TimeScaleConversionResult& result,
        TimeScaleConversionStatus::Type sourceStatus,
        WarningMask sourceWarningCodeMask
    ) noexcept;

    void addWarning(const TimeScaleConversionWarningCode::Type code) noexcept
    {
        warningCodeMask.add(WarningMask::bit(code));
    }

    [[nodiscard]] bool hasWarning(const TimeScaleConversionWarningCode::Type code) const noexcept
    {
        return warningCodeMask.has(WarningMask::bit(code));
    }
};

}  // namespace skygate::ephemeris
