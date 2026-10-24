#pragma once

#include "IDeltaTProvider.hpp"
#include "IEarthOrientationProvider.hpp"
#include "ILeapSecondProvider.hpp"
#include "LeapSecondOffsetResolver.hpp"
#include "TimeScaleConversionResult.hpp"
#include "TimeScaleOffsetLookup.hpp"
#include "TimeScaleServiceOptions.hpp"

#include <memory>

namespace skygate::ephemeris {

class Ut1OffsetResolver final {
public:
    /// The resolved UT1-UTC offset and its report.
    using Result = TimeScaleOffsetLookup;

    Ut1OffsetResolver(
        std::shared_ptr<const ILeapSecondProvider> leapSecondProvider,
        std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider,
        std::shared_ptr<const IDeltaTProvider> deltaTProvider,
        TimeScaleServiceOptions options
    );

    [[nodiscard]] Result lookupUtcToUt1Offset(const skygate::core::AstronomicalEpoch& utcEpoch) const;
    [[nodiscard]] TimeScaleConversionResult lookupUtcFromUt1(const skygate::core::AstronomicalEpoch& ut1Epoch) const;

    static void mergeWarnings(TimeScaleConversionResult& result, const Result& lookup) noexcept;

private:
    std::shared_ptr<const IEarthOrientationProvider> m_earthOrientationProvider;
    std::shared_ptr<const IDeltaTProvider> m_deltaTProvider;
    LeapSecondOffsetResolver m_leapSecondOffsetResolver;
    TimeScaleServiceOptions m_options;
};

}  // namespace skygate::ephemeris
