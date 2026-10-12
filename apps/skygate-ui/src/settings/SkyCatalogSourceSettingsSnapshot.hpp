#pragma once

#include <QString>

namespace skygate::ui::internal {

struct SkyCatalogSourceSettingsSnapshot final {
    int starPresetIndex = 0;
    QString starUrlText;
    int deepSkyPresetIndex = 0;
    QString deepSkyUrlText;
};

}  // namespace skygate::ui::internal
