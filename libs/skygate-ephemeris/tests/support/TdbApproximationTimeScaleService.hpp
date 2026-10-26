#pragma once

#include "engine/ITimeScaleService.hpp"

namespace skygate::ephemeris::tests {

// Reports every epoch conversion as a degraded TDB approximation with the
// TDB-approximation warning so tests can assert that callers either swallow or
// propagate the approximation. Civil date/time conversion is unsupported,
// matching the epoch-only conversion paths that use this double.
class TdbApproximationTimeScaleService final : public skygate::ephemeris::ITimeScaleService {
public:
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, skygate::core::TimeScale targetScale) const override;

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, skygate::core::TimeScale targetScale
    ) const override;

    [[nodiscard]] int convertCallCount() const noexcept;

private:
    mutable int m_convertCallCount = 0;
};

}  // namespace skygate::ephemeris::tests
