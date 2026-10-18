#pragma once

#include "ILeapSecondProvider.hpp"
#include "TimeScaleConversionResult.hpp"
#include "TimeScaleServiceOptions.hpp"

#include <memory>
#include <optional>
#include <string>

namespace skygate::ephemeris {

class LeapSecondOffsetResolver final {
public:
    struct Result {
        std::optional<int> offsetSeconds;
        TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Valid;
        TimeScaleConversionResult::WarningMask warningCodeMask;
        std::string diagnosticText;

        void addWarning(const TimeScaleConversionWarningCode::Type code) noexcept;
    };

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
