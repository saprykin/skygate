#pragma once

#include <cstdint>

namespace skygate::ephemeris {

class TimeScaleConversionWarningCode {
public:
    enum class Type : std::uint8_t {
        LeapSecondTableMissing,
        LeapSecondTableStale,
        EpochOutsideLeapSecondTable,
        LeapSecondFallbackApplied,
        UnsupportedConversion,
        InvalidInput,
        TdbApproximationApplied,
        EarthOrientationDataMissing,
        EarthOrientationDataStale,
        EarthOrientationDataPredicted,
        EpochOutsideEarthOrientationData,
        DeltaTFallbackApplied,
        DeltaTUnavailable,
        EarthOrientationDataEstimated
    };

    TimeScaleConversionWarningCode() = delete;
};

}  // namespace skygate::ephemeris
