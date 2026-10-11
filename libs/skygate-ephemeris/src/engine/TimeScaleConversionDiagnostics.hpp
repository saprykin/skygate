#pragma once

#include "TimeScaleConversionStatus.hpp"
#include "TimeScaleConversionWarningCode.hpp"

#include <string_view>

namespace skygate::ephemeris {

class TimeScaleConversionDiagnostics final {
public:
    [[nodiscard]] static constexpr std::string_view displayName(const TimeScaleConversionStatus::Type status) noexcept
    {
        return TimeScaleConversionStatus::displayName(status);
    }

    [[nodiscard]] static constexpr std::string_view
    warningText(const TimeScaleConversionWarningCode::Type code) noexcept
    {
        switch (code) {
        case TimeScaleConversionWarningCode::Type::LeapSecondTableMissing:
            return "Leap-second table data is unavailable.";
        case TimeScaleConversionWarningCode::Type::LeapSecondTableStale:
            return "Leap-second table data is stale for this conversion.";
        case TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable:
            return "The requested epoch is outside the leap-second table validity range.";
        case TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied:
            return "A degraded leap-second fallback offset was applied.";
        case TimeScaleConversionWarningCode::Type::UnsupportedConversion:
            return "The requested time-scale conversion is not supported.";
        case TimeScaleConversionWarningCode::Type::InvalidInput:
            return "The requested time-scale conversion input is invalid.";
        case TimeScaleConversionWarningCode::Type::TdbApproximationApplied:
            return "The TT/TDB conversion used a documented approximation.";
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing:
            return "Earth-orientation data is unavailable.";
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataStale:
            return "Earth-orientation data is stale for this conversion.";
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted:
            return "Earth-orientation data uses a prediction for this conversion.";
        case TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData:
            return "The requested epoch is outside the Earth-orientation data range.";
        case TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied:
            return "A degraded Delta T fallback was applied.";
        case TimeScaleConversionWarningCode::Type::DeltaTUnavailable:
            return "Delta T data is unavailable for this conversion.";
        case TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated:
            return "Earth-orientation data uses an estimate for this conversion.";
        }

        return "Time-scale conversion warning.";
    }
};

}  // namespace skygate::ephemeris
