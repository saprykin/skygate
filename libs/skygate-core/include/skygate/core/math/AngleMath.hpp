#pragma once

namespace skygate::core {

class AngleMath final {
public:
    [[nodiscard]] static double toRadians(double degrees) noexcept;
    [[nodiscard]] static double toDegrees(double radians) noexcept;
    [[nodiscard]] static double normalizeDegrees(double degrees) noexcept;
    [[nodiscard]] static double normalizeDegreesSigned(double degrees) noexcept;
    [[nodiscard]] static double normalizeHours(double hours) noexcept;
};

}  // namespace skygate::core
