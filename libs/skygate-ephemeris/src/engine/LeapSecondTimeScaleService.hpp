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
    convert(const skygate::core::AstronomicalEpoch& epoch, skygate::core::TimeScale targetScale) const override;

    [[nodiscard]] TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, skygate::core::TimeScale targetScale
    ) const override;

private:
    [[nodiscard]] static double
    targetOffsetSeconds(skygate::core::TimeScale target, double taiMinusUtcSeconds) noexcept;

    // Takes the warning report of a resolved lookup into a completed result.
    // The lookup status and mask merge in, and its diagnostic text replaces
    // the generic text when the lookup has one.
    template <typename Lookup>
    static void applyLookup(TimeScaleConversionResult& result, const Lookup& lookup) noexcept
    {
        TimeScaleConversionResult::mergeWarnings(result, lookup.status, lookup.warningCodeMask);
        if (!lookup.diagnosticText.empty()) {
            result.diagnosticText = lookup.diagnosticText;
        }
    }

    [[nodiscard]] TimeScaleConversionResult convertVia(
        skygate::core::TimeScale intermediateScale,
        const skygate::core::AstronomicalEpoch& epoch,
        skygate::core::TimeScale targetScale
    ) const;

    LeapSecondOffsetResolver m_leapSecondOffsetResolver;
    Ut1OffsetResolver m_ut1OffsetResolver;
};

}  // namespace skygate::ephemeris
