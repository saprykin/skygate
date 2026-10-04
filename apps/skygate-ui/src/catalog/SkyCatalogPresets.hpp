#pragma once

#include "SkyCatalogSourceDescriptor.hpp"

#include <QString>
#include <QVector>

#include <optional>

namespace skygate::ui::internal {

class SkyCatalogPresets final {
public:
    // Legacy preset-index normalization retained for the settings/cache
    // migration path. New sources are addressed by stable source IDs.
    [[nodiscard]] static int normalizeCatalogPresetIndex(int presetIndex) noexcept;
    [[nodiscard]] static int normalizeDeepSkyCatalogPresetIndex(int presetIndex) noexcept;
    [[nodiscard]] static QString defaultCatalogUrlText();
    [[nodiscard]] static QString defaultDeepSkyCatalogUrlText();
    [[nodiscard]] static QString starCategory();
    [[nodiscard]] static QString deepSkyCategory();
    [[nodiscard]] static std::optional<SkyCatalogSourceDescriptor> starSourceDescriptor(const QString& presetId);
    [[nodiscard]] static std::optional<SkyCatalogSourceDescriptor> deepSkySourceDescriptor(const QString& presetId);
    [[nodiscard]] static QVector<SkyCatalogSourceDescriptor> starSourceDescriptors();
    [[nodiscard]] static QVector<SkyCatalogSourceDescriptor> deepSkySourceDescriptors();
};

}  // namespace skygate::ui::internal
