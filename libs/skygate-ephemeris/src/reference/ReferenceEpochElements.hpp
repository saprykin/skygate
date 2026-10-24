#pragma once

#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

// Epoch and frame parameters shared by every mean-element calculator of the
// reference model. The day count is the argument of the model's element
// polynomials and the mean obliquity rotates the resulting ecliptic
// coordinates into the equatorial frame, so both live here instead of being
// re-derived by each calculator.
class ReferenceEpochElements final {
public:
    ReferenceEpochElements() = delete;

    // Days elapsed since J2000.0, the argument of the reference mean elements.
    [[nodiscard]] static double daysSinceJ2000(const skygate::core::UtcTimePoint& utcTime) noexcept;

    // Mean obliquity of the ecliptic for an instant given as days since J2000.0.
    [[nodiscard]] static double meanObliquityDeg(double daysSinceJ2000) noexcept;
};

}  // namespace skygate::ephemeris
