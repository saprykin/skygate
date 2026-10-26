#pragma once

#include <QString>

namespace skygate::ui::internal {

class SkyContextTextFormatter final {
public:
    [[nodiscard]] static QString formatFixed(double value, int precision);
    [[nodiscard]] static QString formatCoordinate(double value);
    [[nodiscard]] static QString formatElevation(double value);
};

}  // namespace skygate::ui::internal
