#pragma once

#include <QString>

namespace skygate::ui::internal {

// The legacy two-slot catalog controls: a star slot and a deep-sky slot, each
// with a preset index and a URL text. The QML preferences surface still
// round-trips these values and a restore uses them to migrate an installation
// whose collection was saved by the retired two-slot controls.
class SkyCatalogLegacySlot final {
public:
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

private:
    int m_catalogPresetIndex = 0;
    int m_deepSkyCatalogPresetIndex = 0;
    QString m_catalogUrlText;
    QString m_deepSkyCatalogUrlText;
};

}  // namespace skygate::ui::internal
