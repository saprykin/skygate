#pragma once

#include "SkyCatalogSourceDescriptor.hpp"

#include "catalog/CatalogCompositionPolicy.hpp"

#include <QString>
#include <QVector>

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ui::internal {

class SkyCatalogPresets final {
public:
    // Legacy preset index of the custom-URL entry: the bundled source and the
    // preset source own the indexes before it.
    static constexpr int kCustomLegacyPresetIndex = 2;

    // Legacy preset-index normalization retained for the settings/cache
    // migration path. New sources are addressed by stable source IDs.
    [[nodiscard]] static int normalizeCatalogPresetIndex(int presetIndex) noexcept;
    [[nodiscard]] static int normalizeDeepSkyCatalogPresetIndex(int presetIndex) noexcept;
    [[nodiscard]] static QString defaultCatalogUrlText();
    [[nodiscard]] static QString defaultDeepSkyCatalogUrlText();
    [[nodiscard]] static QString starCategory();
    [[nodiscard]] static QString deepSkyCategory();
    // Composition policy a source of this category participates with. Only the
    // deep-sky category composes as a deep-sky catalog.
    [[nodiscard]] static skygate::ephemeris::CatalogCompositionPolicy policyForCategory(const QString& category);
    // Category of a source that participates with this composition policy.
    [[nodiscard]] static QString categoryForPolicy(skygate::ephemeris::CatalogCompositionPolicy policy);
    [[nodiscard]] static std::optional<SkyCatalogSourceDescriptor> starSourceDescriptor(const QString& presetId);
    [[nodiscard]] static std::optional<SkyCatalogSourceDescriptor> deepSkySourceDescriptor(const QString& presetId);
    [[nodiscard]] static QVector<SkyCatalogSourceDescriptor> starSourceDescriptors();
    [[nodiscard]] static QVector<SkyCatalogSourceDescriptor> deepSkySourceDescriptors();

private:
    // Preset IDs arrive from stored settings and user input, so they are
    // matched the way catalog identities are: ASCII whitespace trimmed and
    // ASCII case folded to lower case. An ID that carries any other character
    // keeps it, which is what makes such a request miss the preset tables.
    [[nodiscard]] static std::string normalizedPresetId(std::string_view presetId);
    [[nodiscard]] static bool presetIdMatches(std::string_view registeredId, std::string_view requestedId);
};

}  // namespace skygate::ui::internal
