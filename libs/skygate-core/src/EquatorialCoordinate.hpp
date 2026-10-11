#pragma once

namespace skygate::core {

struct EquatorialCoordinate {
    static constexpr double kRightAscensionMinHours = 0.0;
    // The upper bound is exclusive: 24.0 hours wraps to 0.0 hours.
    static constexpr double kRightAscensionMaxHours = 24.0;
    static constexpr double kDeclinationMinDeg = -90.0;
    static constexpr double kDeclinationMaxDeg = 90.0;

    double rightAscensionHours = 0.0;
    double declinationDeg = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
};

}  // namespace skygate::core
