#pragma once

#include "SkyCatalogSourceDescriptor.hpp"

#include <QString>
#include <QStringList>

namespace skygate::ui::internal {

// A configured source instance. The instance ID is the durable identity and is
// independent of the display title; the descriptor ID records which preset
// template, if any, the instance was configured from. Two custom sources or two
// versions of the same preset can therefore coexist without collapsing.
struct SkyCatalogSourceInstance final {
    QString instanceId;
    QString descriptorId;
    QString title;
    QString version;
    QStringList urls;
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
    QString archiveSelector;
    QStringList relatedDatasetUrls;
    QString attribution;

    [[nodiscard]] static SkyCatalogSourceInstance fromDescriptor(const SkyCatalogSourceDescriptor& descriptor);
    [[nodiscard]] static SkyCatalogSourceInstance createCustom(const QString& urlText, const QString& version = {});

    [[nodiscard]] bool isCustom() const noexcept;
};

}  // namespace skygate::ui::internal
