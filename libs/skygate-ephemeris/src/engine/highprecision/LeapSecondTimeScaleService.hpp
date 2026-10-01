#pragma once

#include "IDeltaTProvider.hpp"
#include "ILeapSecondProvider.hpp"
#include "ITimeScaleService.hpp"
#include "TimeScaleServiceOptions.hpp"

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
    std::shared_ptr<const ILeapSecondProvider> m_leapSecondProvider;
    std::shared_ptr<const IEarthOrientationProvider> m_earthOrientationProvider;
    std::shared_ptr<const IDeltaTProvider> m_deltaTProvider;
    TimeScaleServiceOptions m_options;
};

}  // namespace skygate::ephemeris
