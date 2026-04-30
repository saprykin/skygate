#include "SkyCatalogPresets.hpp"

#include "SkyContextControllerSupport.hpp"

namespace skygate::ui::internal {

int SkyCatalogPresets::normalizeCatalogPresetIndex(const int presetIndex) noexcept
{
    if (presetIndex <= 0) {
        return 0;
    }
    if (presetIndex == 1 || presetIndex == 3) {
        return 1;
    }
    return 2;
}

int SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(const int presetIndex) noexcept
{
    if (presetIndex <= 0) {
        return 0;
    }
    if (presetIndex == 1) {
        return 1;
    }
    return 2;
}

QString SkyCatalogPresets::defaultCatalogUrlText()
{
    return QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl);
}

QString SkyCatalogPresets::defaultDeepSkyCatalogUrlText()
{
    return QString::fromUtf8(SkyContextControllerConstants::kOpenNgcCatalogPrimaryUrl);
}

SkyCatalogPreset SkyCatalogPresets::catalogPreset(const QString& presetId)
{
    const QString normalizedPresetId = presetId.trimmed().toLower();
    if (normalizedPresetId == "bundled") {
        SkyCatalogPreset preset;
        preset.known = true;
        preset.bundled = true;
        preset.presetIndex = 0;
        preset.sourceLabel = "Bundled";
        return preset;
    }

    if (normalizedPresetId == "hyg_v42" || normalizedPresetId == "hyg_v3") {
        SkyCatalogPreset preset;
        preset.known = true;
        preset.presetIndex = 1;
        preset.defaultUrlText = defaultCatalogUrlText();
        preset.sourceLabel = "HYG v4.2";
        preset.catalogUrls = QStringList {preset.defaultUrlText};
        preset.constellationLineUrls = QStringList {
            QString::fromUtf8(
                SkyContextControllerConstants::kStellariumConstellationLinesPrimaryUrl
            ),
            QString::fromUtf8(
                SkyContextControllerConstants::kStellariumConstellationLinesMirrorUrl
            ),
            QString::fromUtf8(SkyContextControllerConstants::kStellariumConstellationLinesCdnUrl)
        };
        return preset;
    }

    return {};
}

SkyDeepSkyCatalogPreset SkyCatalogPresets::deepSkyCatalogPreset(const QString& presetId)
{
    const QString normalizedPresetId = presetId.trimmed().toLower();
    if (normalizedPresetId == "bundled_messier" || normalizedPresetId == "bundled") {
        SkyDeepSkyCatalogPreset preset;
        preset.known = true;
        preset.bundled = true;
        preset.presetIndex = 0;
        preset.sourceLabel = "Bundled Messier";
        return preset;
    }

    if (normalizedPresetId == "open_ngc") {
        SkyDeepSkyCatalogPreset preset;
        preset.known = true;
        preset.presetIndex = 1;
        preset.defaultUrlText = defaultDeepSkyCatalogUrlText();
        preset.sourceLabel = "OpenNGC";
        preset.catalogUrls = QStringList {
            QString::fromUtf8(SkyContextControllerConstants::kOpenNgcCatalogPrimaryUrl),
            QString::fromUtf8(SkyContextControllerConstants::kOpenNgcCatalogMirrorUrl)
        };
        return preset;
    }

    return {};
}

}  // namespace skygate::ui::internal
