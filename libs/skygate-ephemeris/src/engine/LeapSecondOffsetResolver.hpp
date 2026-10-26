#pragma once

#include "ILeapSecondProvider.hpp"
#include "TimeScaleOffsetLookup.hpp"
#include "TimeScaleServiceOptions.hpp"

#include <memory>

namespace skygate::ephemeris {

class LeapSecondOffsetResolver final {
public:
    /// The resolved TAI-UTC step count and its report.
    using Result = TimeScaleOffsetLookup;

    LeapSecondOffsetResolver(
        std::shared_ptr<const ILeapSecondProvider> leapSecondProvider, TimeScaleServiceOptions options
    );

    [[nodiscard]] Result lookupUtcOffset(const skygate::core::AstronomicalEpoch& utcEpoch) const;
    [[nodiscard]] Result lookupTaiOffset(const skygate::core::AstronomicalEpoch& taiEpoch) const;

private:
    std::shared_ptr<const ILeapSecondProvider> m_leapSecondProvider;
    TimeScaleServiceOptions m_options;
};

}  // namespace skygate::ephemeris
