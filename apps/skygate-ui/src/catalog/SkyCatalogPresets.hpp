#pragma once

#include <QString>
#include <QStringList>

namespace skygate::ui::internal {

struct SkyCatalogPreset final {
    bool known = false;
    bool bundled = false;
    int presetIndex = 0;
    QString defaultUrlText;
    QString sourceLabel;
    QStringList catalogUrls;
    QStringList constellationLineUrls;
};

struct SkyDeepSkyCatalogPreset final {
    bool known = false;
    bool bundled = false;
    int presetIndex = 0;
    QString defaultUrlText;
    QString sourceLabel;
    QStringList catalogUrls;
};

class SkyCatalogPresets final {
public:
    [[nodiscard]] static int normalizeCatalogPresetIndex(int presetIndex) noexcept;
    [[nodiscard]] static int normalizeDeepSkyCatalogPresetIndex(int presetIndex) noexcept;
    [[nodiscard]] static QString defaultCatalogUrlText();
    [[nodiscard]] static QString defaultDeepSkyCatalogUrlText();
    [[nodiscard]] static SkyCatalogPreset catalogPreset(const QString& presetId);
    [[nodiscard]] static SkyDeepSkyCatalogPreset deepSkyCatalogPreset(const QString& presetId);
};

}  // namespace skygate::ui::internal
