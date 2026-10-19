#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QString>

#include <optional>

namespace skygate::ui::internal {

// The legacy two-slot catalog controls: a star slot and a deep-sky slot, each
// with a preset index and a URL text. The QML preferences surface still
// addresses the collection through them, while new sources are added by preset
// ID or URL.
//
// The four entry points resolve a preset ID or a URL into the source instance
// the manager then activates. A request that names an unknown preset selects
// nothing and carries the status text that reports it.
class SkyCatalogLegacySlot final {
public:
    // Outcome of a legacy slot entry point: the source to activate together
    // with its composition policy, or the status text of a request that
    // resolved to no source.
    struct Selection final {
        std::optional<SkyCatalogSourceInstance> source;
        skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        QString statusText;
    };

    SkyCatalogLegacySlot();

    [[nodiscard]] int catalogPresetIndex() const noexcept
    {
        return m_catalogPresetIndex;
    }

    void setCatalogPresetIndex(int catalogPresetIndex);

    [[nodiscard]] const QString& catalogUrlText() const noexcept
    {
        return m_catalogUrlText;
    }

    void setCatalogUrlText(const QString& catalogUrlText);

    [[nodiscard]] int deepSkyCatalogPresetIndex() const noexcept
    {
        return m_deepSkyCatalogPresetIndex;
    }

    void setDeepSkyCatalogPresetIndex(int deepSkyCatalogPresetIndex);

    [[nodiscard]] const QString& deepSkyCatalogUrlText() const noexcept
    {
        return m_deepSkyCatalogUrlText;
    }

    void setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText);

    // Records the selection the request resolved and reports the instance to
    // activate. The selected instance carries the fixed legacy slot ID.
    [[nodiscard]] Selection selectCatalogPreset(const QString& presetId);
    [[nodiscard]] Selection selectDeepSkyCatalogPreset(const QString& presetId);
    [[nodiscard]] Selection selectCatalogUrl(const QString& urlText);
    [[nodiscard]] Selection selectDeepSkyCatalogUrl(const QString& urlText);

private:
    int m_catalogPresetIndex = 0;
    int m_deepSkyCatalogPresetIndex = 0;
    QString m_catalogUrlText;
    QString m_deepSkyCatalogUrlText;
};

}  // namespace skygate::ui::internal
