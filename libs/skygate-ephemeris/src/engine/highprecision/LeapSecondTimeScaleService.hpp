#pragma once

#include "IDeltaTProvider.hpp"
#include "IEarthOrientationProvider.hpp"
#include "ILeapSecondProvider.hpp"
#include "ITimeScaleService.hpp"
#include "LeapSecondOffsetResolver.hpp"
#include "TimeScaleServiceOptions.hpp"
#include "Ut1OffsetResolver.hpp"

#include <memory>

namespace skygate::ephemeris {

class LeapSecondTimeScaleService final : public ITimeScaleService {
public:
    explicit LeapSecondTimeScaleService(
        std::shared_ptr<const ILeapSecondProvider> leapSecondProvider,
        TimeScaleServiceOptions options = {},
        std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider = nullptr,
        std::shared_ptr<const IDeltaTProvider> deltaTProvider = nullptr
    );

    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, TimeScale targetScale) const override;

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, TimeScale targetScale) const override;

private:
    LeapSecondOffsetResolver m_leapSecondOffsetResolver;
    Ut1OffsetResolver m_ut1OffsetResolver;
};

}  // namespace skygate::ephemeris
