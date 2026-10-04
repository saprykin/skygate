#pragma once

#include "catalog/CatalogSourceType.hpp"

#include <QString>
#include <QStringList>

namespace skygate::ui::internal {

// Stable identity and presentation metadata for one catalog source preset.
// The source ID is the durable identity; the title is presentation text and may
// change independently of the ID.
struct SkyCatalogSourceDescriptor final {
    QString sourceId;
    QString title;
    QString version;
    QStringList urls;
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
    QString archiveSelector;
    QStringList relatedDatasetUrls;
    QString attribution;
    QString category;
    int legacyPresetIndex = 0;
    bool bundled = false;

    [[nodiscard]] bool isKnown() const noexcept;
    [[nodiscard]] QString defaultUrl() const;
};

}  // namespace skygate::ui::internal
