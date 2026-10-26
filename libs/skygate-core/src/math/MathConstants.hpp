#pragma once

#include <numbers>

namespace skygate::core {

class MathConstants final {
public:
    static constexpr double kPi = std::numbers::pi_v<double>;
    static constexpr double kTwoPi = 2.0 * kPi;
    static constexpr double kDegreesToRadians = kPi / 180.0;
    static constexpr double kRadiansToDegrees = 180.0 / kPi;
    static constexpr double kDegreesPerTurn = 360.0;
    static constexpr double kHoursPerTurn = 24.0;
    static constexpr double kPolarAngleDeg = kDegreesPerTurn / 4.0;
    static constexpr double kDegreesPerHour = kDegreesPerTurn / kHoursPerTurn;
    static constexpr double kRadiansPerHour = kTwoPi / kHoursPerTurn;
    static constexpr double kHoursPerRadian = kHoursPerTurn / kTwoPi;
    static constexpr double kMilliarcsecondsPerArcsecond = 1'000.0;
    static constexpr double kArcminutesPerDegree = 60.0;
    static constexpr double kArcsecondsToRadians = kPi / (180.0 * 3600.0);
    static constexpr double kMilliarcsecondsToRadians = kArcsecondsToRadians / kMilliarcsecondsPerArcsecond;
    static constexpr double kEpsilon = 1e-12;
    // Screen-space tolerance for line building. It is separate from kEpsilon,
    // the unit-sphere guard, because pixel lengths are not bounded by one.
    static constexpr double kMinSegmentLengthPx = 1e-9;
};

}  // namespace skygate::core
