#pragma once

#include <QString>

namespace skygate::ui::internal {

class SkyContextTextFormatter final {
public:
    [[nodiscard]] static QString formatCoordinate(double value);
    [[nodiscard]] static QString formatElevation(double value);
};

}  // namespace skygate::ui::internal
