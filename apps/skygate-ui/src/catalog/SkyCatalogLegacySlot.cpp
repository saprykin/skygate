#include "SkyCatalogLegacySlot.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceIds.hpp"
#include "SkyCatalogText.hpp"

#include <optional>
#include <utility>

namespace skygate::ui::internal {

SkyCatalogLegacySlot::SkyCatalogLegacySlot()
    : m_catalogUrlText(SkyCatalogPresets::defaultCatalogUrlText()),
      m_deepSkyCatalogUrlText(SkyCatalogPresets::defaultDeepSkyCatalogUrlText())
{
}

void SkyCatalogLegacySlot::setCatalogPresetIndex(const int catalogPresetIndex)
{
    m_catalogPresetIndex = SkyCatalogPresets::normalizeCatalogPresetIndex(catalogPresetIndex);
}

void SkyCatalogLegacySlot::setCatalogUrlText(const QString& catalogUrlText)
{
    const QString normalizedCatalogUrlText = catalogUrlText.trimmed();
    m_catalogUrlText =
        normalizedCatalogUrlText.isEmpty() ? SkyCatalogPresets::defaultCatalogUrlText() : normalizedCatalogUrlText;
}

void SkyCatalogLegacySlot::setDeepSkyCatalogPresetIndex(const int deepSkyCatalogPresetIndex)
{
    m_deepSkyCatalogPresetIndex = SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(deepSkyCatalogPresetIndex);
}

void SkyCatalogLegacySlot::setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText)
{
    const QString normalizedUrlText = deepSkyCatalogUrlText.trimmed();
    m_deepSkyCatalogUrlText =
        normalizedUrlText.isEmpty() ? SkyCatalogPresets::defaultDeepSkyCatalogUrlText() : normalizedUrlText;
}

SkyCatalogLegacySlot::Selection SkyCatalogLegacySlot::selectCatalogPreset(const QString& presetId)
{
    const std::optional<SkyCatalogSourceDescriptor> descriptor = SkyCatalogPresets::starSourceDescriptor(presetId);
    if (!descriptor.has_value()) {
        return Selection{.statusText = SkyCatalogText::unknownCatalogPreset(presetId)};
    }

    setCatalogPresetIndex(descriptor->legacyPresetIndex);
    auto source = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
    source.instanceId = QString::fromLatin1(SkyCatalogSourceIds::kPrimarySlot);
    if (!descriptor->bundled) {
        setCatalogUrlText(descriptor->defaultUrl());
    }
    return Selection{.source = std::move(source)};
}

SkyCatalogLegacySlot::Selection SkyCatalogLegacySlot::selectDeepSkyCatalogPreset(const QString& presetId)
{
    const std::optional<SkyCatalogSourceDescriptor> descriptor = SkyCatalogPresets::deepSkySourceDescriptor(presetId);
    if (!descriptor.has_value()) {
        return Selection{.statusText = SkyCatalogText::unknownDeepSkyPreset(presetId)};
    }

    setDeepSkyCatalogPresetIndex(descriptor->legacyPresetIndex);
    auto source = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
    source.instanceId = QString::fromLatin1(SkyCatalogSourceIds::kDeepSkySlot);
    if (!descriptor->bundled) {
        setDeepSkyCatalogUrlText(descriptor->defaultUrl());
    }
    return Selection{
        .source = std::move(source),
        .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
    };
}

SkyCatalogLegacySlot::Selection SkyCatalogLegacySlot::selectCatalogUrl(const QString& urlText)
{
    setCatalogPresetIndex(SkyCatalogPresets::kCustomLegacyPresetIndex);
    setCatalogUrlText(urlText);
    auto source = SkyCatalogSourceInstance::createCustom(urlText);
    source.instanceId = QString::fromLatin1(SkyCatalogSourceIds::kPrimarySlot);
    return Selection{.source = std::move(source)};
}

SkyCatalogLegacySlot::Selection SkyCatalogLegacySlot::selectDeepSkyCatalogUrl(const QString& urlText)
{
    setDeepSkyCatalogPresetIndex(SkyCatalogPresets::kCustomLegacyPresetIndex);
    setDeepSkyCatalogUrlText(urlText);
    auto source = SkyCatalogSourceInstance::createCustom(urlText);
    source.instanceId = QString::fromLatin1(SkyCatalogSourceIds::kDeepSkySlot);
    return Selection{
        .source = std::move(source),
        .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
    };
}

}  // namespace skygate::ui::internal
