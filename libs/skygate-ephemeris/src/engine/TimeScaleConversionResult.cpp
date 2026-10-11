#include "TimeScaleConversionResult.hpp"

#include <utility>

namespace skygate::ephemeris {

TimeScaleConversionResult TimeScaleConversionResult::success(
    skygate::core::AstronomicalEpoch epoch,
    const TimeScaleConversionStatus status,
    const std::uint32_t warningCodeMask,
    std::string diagnosticText
)
{
    if (diagnosticText.empty()) {
        diagnosticText = status == TimeScaleConversionStatus::Valid ? "Time-scale conversion succeeded."
                                                                    : "Time-scale conversion degraded.";
    }

    return TimeScaleConversionResult{
        .epoch = epoch.normalized(),
        .status = status,
        .warningCodeMask = warningCodeMask,
        .diagnosticText = std::move(diagnosticText),
    };
}

TimeScaleConversionResult TimeScaleConversionResult::failure(
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale,
    std::string diagnosticText
)
{
    TimeScaleConversionResult result;
    result.epoch = epoch;
    result.epoch.timeScale = targetScale;
    result.status = TimeScaleConversionStatus::Failed;
    result.diagnosticText = std::move(diagnosticText);
    return result;
}

void TimeScaleConversionResult::mergeWarnings(
    TimeScaleConversionResult& result,
    const TimeScaleConversionStatus sourceStatus,
    const std::uint32_t sourceWarningCodeMask
) noexcept
{
    result.warningCodeMask |= sourceWarningCodeMask;
    if (sourceStatus == TimeScaleConversionStatus::Degraded && result.status == TimeScaleConversionStatus::Valid) {
        result.status = TimeScaleConversionStatus::Degraded;
    }
}

}  // namespace skygate::ephemeris
