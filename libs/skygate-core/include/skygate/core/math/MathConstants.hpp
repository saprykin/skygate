#pragma once

#include <numbers>

namespace skygate::core {

class MathConstants final {
public:
    static constexpr double kPi = std::numbers::pi_v<double>;
    static constexpr double kDegreesToRadians = kPi / 180.0;
    static constexpr double kRadiansToDegrees = 180.0 / kPi;
    static constexpr double kEpsilon = 1e-12;
};

}  // namespace skygate::core
