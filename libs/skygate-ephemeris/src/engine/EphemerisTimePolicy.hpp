#pragma once

namespace skygate::ephemeris {

// Numeric policy shared by the time-scale and table-parsing paths. The values
// are tuning decisions kept in one place instead of being retyped at each call
// site.
class EphemerisTimePolicy final {
public:
    EphemerisTimePolicy() = delete;

    // Refinement rounds used to invert the UT1 minus UTC offset, where the
    // offset has to be sampled at the UTC epoch being solved for.
    static constexpr int kUt1InverseIterations = 4;
    // Refinement rounds used to invert the TDB minus TT approximation, where
    // the offset has to be evaluated at the TT epoch being solved for.
    static constexpr int kTdbInverseIterations = 3;
    // Two epoch keys that differ by less than this many days name the same
    // sampled instant.
    static constexpr double kEpochKeyEqualityToleranceDays = 1.0e-9;
    // Modified Julian Date at which a two-digit table year flips from the
    // 1900s to the 2000s.
    static constexpr double kTwoDigitYearPivotMjd = 51543.0;
};

}  // namespace skygate::ephemeris
