#include "ValidUtcTimeScaleService.hpp"

#include "engine/TimeScaleConversionStatus.hpp"

namespace skygate::ephemeris::tests {

using namespace skygate::core;

TimeScaleConversionResult ValidUtcTimeScaleService::convert(
    const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale
) const
{
    ++m_convertCallCount;
    m_lastTargetScale = targetScale;
    return {
        .epoch =
            skygate::core::AstronomicalEpoch{
                .julianDatePart1 = epoch.julianDatePart1,
                .julianDatePart2 = epoch.julianDatePart2,
                .timeScale = targetScale,
            }
                .normalized(),
        .status = TimeScaleConversionStatus::Valid,
        .diagnosticText = "unit test conversion",
    };
}

TimeScaleConversionResult ValidUtcTimeScaleService::convertCivilDateTime(
    const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
) const
{
    static_cast<void>(dateTime);
    static_cast<void>(targetScale);
    return {};
}

int ValidUtcTimeScaleService::convertCallCount() const noexcept
{
    return m_convertCallCount;
}

skygate::core::TimeScale ValidUtcTimeScaleService::lastTargetScale() const noexcept
{
    return m_lastTargetScale;
}

}  // namespace skygate::ephemeris::tests
