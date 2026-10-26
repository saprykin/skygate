#pragma once

#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>
#include <QString>

#include <span>

namespace skygate::ui::internal {

// Cardinal directions of the horizon. One table owns the azimuth of the
// direction, the label text, and the palette color shared by the line and the
// label that mark it.
class SkyCardinalDirectionPolicy final {
public:
    struct Direction final {
        double azimuthDeg = 0.0;
        const char* label = "";
        // Palette member that colors the direction. Every direction the policy
        // hands out comes from the table below, which sets all three fields.
        QColor SkyThemeRenderPalette::* lineColor = nullptr;

        [[nodiscard]] QString text() const
        {
            return QString::fromLatin1(label);
        }

        [[nodiscard]] QColor color(const SkyThemeRenderPalette& renderPalette) const noexcept
        {
            return renderPalette.*lineColor;
        }
    };

    SkyCardinalDirectionPolicy() = delete;

    [[nodiscard]] static std::span<const Direction> directions() noexcept;
};

}  // namespace skygate::ui::internal
