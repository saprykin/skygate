#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class TimeScaleConversionWarningCode : std::uint8_t {
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

}  // namespace skygate::ephemeris
