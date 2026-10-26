#pragma once

#include "TimeScaleConversionResult.hpp"

#include <optional>
#include <string>

namespace skygate::ephemeris {

/// Value and report of one resolved time-scale offset lookup. The leap-second
/// resolver stores the TAI-UTC step count and the UT1 resolver stores the
/// UT1-UTC offset, both in seconds.
struct TimeScaleOffsetLookup {
    std::optional<double> offsetSeconds;
    TimeScaleConversionStatus::Type status = TimeScaleConversionStatus::Type::Valid;
    TimeScaleConversionResult::WarningMask warningCodeMask;
    std::string diagnosticText;

    void addWarning(const TimeScaleConversionWarningCode::Type code) noexcept
    {
        warningCodeMask.add(TimeScaleConversionResult::WarningMask::bit(code));
    }

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == TimeScaleConversionStatus::Type::Valid || status == TimeScaleConversionStatus::Type::Degraded;
    }
};

}  // namespace skygate::ephemeris
