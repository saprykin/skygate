#include "SkyCatalogLegacySlot.hpp"
#include "SkyCatalogPresets.hpp"

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

}  // namespace skygate::ui::internal
