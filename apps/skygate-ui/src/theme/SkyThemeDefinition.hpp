#pragma once

#include "SkyThemeRenderPalette.hpp"
#include "SkyThemeUiPalette.hpp"

#include <QString>

namespace skygate::ui::internal {

struct SkyThemeDefinition final {
    QString id;
    QString displayName;
    SkyThemeUiPalette ui;
    SkyThemeRenderPalette render;

    [[nodiscard]] static SkyThemeDefinition builtInDefault();
};

}  // namespace skygate::ui::internal
