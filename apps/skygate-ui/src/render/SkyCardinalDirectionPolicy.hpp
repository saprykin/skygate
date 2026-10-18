#pragma once

#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>
#include <QString>

#include <array>

namespace skygate::ui::internal {

// Cardinal directions of the horizon: their azimuths, the label text, and the
// palette color of the line and label that mark each direction.
class SkyCardinalDirectionPolicy final {
public:
    static constexpr std::array<double, 4> kAzimuthsDeg{0.0, 90.0, 180.0, 270.0};

    SkyCardinalDirectionPolicy() = delete;

    [[nodiscard]] static QColor lineColor(double azimuthDeg, const SkyThemeRenderPalette& renderPalette);
    [[nodiscard]] static QString label(double azimuthDeg);
};

}  // namespace skygate::ui::internal
