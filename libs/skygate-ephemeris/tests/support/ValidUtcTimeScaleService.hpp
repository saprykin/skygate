#pragma once

#include "engine/ITimeScaleService.hpp"

namespace skygate::ephemeris::tests {

// Converts epochs between time scales while keeping the same instant.
class ValidUtcTimeScaleService final : public skygate::ephemeris::ITimeScaleService {
public:
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, skygate::core::TimeScale targetScale) const override;

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, skygate::core::TimeScale targetScale
    ) const override;

    [[nodiscard]] int convertCallCount() const noexcept;
    [[nodiscard]] skygate::core::TimeScale lastTargetScale() const noexcept;

private:
    mutable int m_convertCallCount = 0;
    mutable skygate::core::TimeScale m_lastTargetScale = skygate::core::TimeScale::Utc;
};

}  // namespace skygate::ephemeris::tests
