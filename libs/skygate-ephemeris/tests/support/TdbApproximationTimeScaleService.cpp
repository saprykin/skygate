#include "TdbApproximationTimeScaleService.hpp"

#include "engine/TimeScaleConversionStatus.hpp"

namespace skygate::ephemeris::tests {

using namespace skygate::core;

TimeScaleConversionResult TdbApproximationTimeScaleService::convert(
    const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale
) const
{
    ++m_convertCallCount;

    TimeScaleConversionResult result;
    result.epoch = epoch;
    result.epoch.timeScale = targetScale;
    result.status = TimeScaleConversionStatus::Type::Degraded;
    result.addWarning(TimeScaleConversionWarningCode::Type::TdbApproximationApplied);
    return result;
}

TimeScaleConversionResult TdbApproximationTimeScaleService::convertCivilDateTime(
    const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
) const
{
    static_cast<void>(dateTime);
    static_cast<void>(targetScale);
    return {};
}

int TdbApproximationTimeScaleService::convertCallCount() const noexcept
{
    return m_convertCallCount;
}

}  // namespace skygate::ephemeris::tests
