#pragma once

#include "TimeScaleConversionStatus.hpp"
#include "TimeScaleConversionWarningCode.hpp"

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class TimeScaleConversionDiagnostics final {
public:
    [[nodiscard]] static constexpr std::string_view displayName(const TimeScaleConversionStatus status) noexcept
    {
        switch (status) {
        case TimeScaleConversionStatus::Valid:
            return "valid";
        case TimeScaleConversionStatus::Degraded:
            return "degraded";
        case TimeScaleConversionStatus::Failed:
            return "failed";
        }

        return {};
    }

    [[nodiscard]] static constexpr std::string_view warningText(const TimeScaleConversionWarningCode code) noexcept
    {
        switch (code) {
        case TimeScaleConversionWarningCode::LeapSecondTableMissing:
            return "Leap-second table data is unavailable.";
        case TimeScaleConversionWarningCode::LeapSecondTableStale:
            return "Leap-second table data is stale for this conversion.";
        case TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable:
            return "The requested epoch is outside the leap-second table validity range.";
        case TimeScaleConversionWarningCode::LeapSecondFallbackApplied:
            return "A degraded leap-second fallback offset was applied.";
        case TimeScaleConversionWarningCode::UnsupportedConversion:
            return "The requested time-scale conversion is not supported.";
        case TimeScaleConversionWarningCode::InvalidInput:
            return "The requested time-scale conversion input is invalid.";
        case TimeScaleConversionWarningCode::TdbApproximationApplied:
            return "The TT/TDB conversion used a documented approximation.";
        case TimeScaleConversionWarningCode::EarthOrientationDataMissing:
            return "Earth-orientation data is unavailable.";
        case TimeScaleConversionWarningCode::EarthOrientationDataStale:
            return "Earth-orientation data is stale for this conversion.";
        case TimeScaleConversionWarningCode::EarthOrientationDataPredicted:
            return "Earth-orientation data uses a prediction for this conversion.";
        case TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData:
            return "The requested epoch is outside the Earth-orientation data range.";
        case TimeScaleConversionWarningCode::DeltaTFallbackApplied:
            return "A degraded Delta T fallback was applied.";
        case TimeScaleConversionWarningCode::DeltaTUnavailable:
            return "Delta T data is unavailable for this conversion.";
        case TimeScaleConversionWarningCode::EarthOrientationDataEstimated:
            return "Earth-orientation data uses an estimate for this conversion.";
        }

        return "Time-scale conversion warning.";
    }

    [[nodiscard]] static constexpr std::uint32_t warningMask(const TimeScaleConversionWarningCode code) noexcept
    {
        return 1U << static_cast<std::uint8_t>(code);
    }
};

}  // namespace skygate::ephemeris
