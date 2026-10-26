#include "TimeScaleConversionResult.hpp"

#include <utility>

namespace skygate::ephemeris {

TimeScaleConversionResult TimeScaleConversionResult::success(
    skygate::core::AstronomicalEpoch epoch,
    const TimeScaleConversionStatus::Type status,
    const WarningMask warningCodeMask,
    std::string diagnosticText
)
{
    if (diagnosticText.empty()) {
        diagnosticText = status == TimeScaleConversionStatus::Type::Valid ? "Time-scale conversion succeeded."
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
    result.status = TimeScaleConversionStatus::Type::Failed;
    result.diagnosticText = std::move(diagnosticText);
    return result;
}

TimeScaleConversionResult TimeScaleConversionResult::failureFromLookup(
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale,
    const WarningMask warningCodeMask,
    std::string diagnosticText
)
{
    TimeScaleConversionResult result = failure(epoch, targetScale, std::move(diagnosticText));
    result.warningCodeMask = warningCodeMask;
    return result;
}

void TimeScaleConversionResult::mergeWarnings(
    TimeScaleConversionResult& result,
    const TimeScaleConversionStatus::Type sourceStatus,
    const WarningMask sourceWarningCodeMask
) noexcept
{
    result.warningCodeMask.merge(sourceWarningCodeMask);
    if (sourceStatus == TimeScaleConversionStatus::Type::Degraded
        && result.status == TimeScaleConversionStatus::Type::Valid) {
        result.status = TimeScaleConversionStatus::Type::Degraded;
    }
}

}  // namespace skygate::ephemeris
