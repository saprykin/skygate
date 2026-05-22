#pragma once

namespace skygate::core {

struct EquatorialCoordinate {
    double rightAscensionHours = 0.0;
    double declinationDeg = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
};

}  // namespace skygate::core
