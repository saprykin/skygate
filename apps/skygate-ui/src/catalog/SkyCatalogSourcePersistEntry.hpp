#pragma once

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>

namespace skygate::ui::internal {

struct SkyCatalogSourcePersistEntry final {
    QString instanceId;
    QString descriptorId;
    QString title;
    QString version;
    QString url;
    QStringList urls;
    QStringList relatedDatasetUrls;
    QString archiveSelector;
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
    QString attribution;
    skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bool enabled = true;
    // Marks a record whose catalog is reconstructed from the bundled factory
    // instead of a payload sidecar.
    bool bundled = false;
    const skygate::ephemeris::IStarCatalog* catalog = nullptr;
    QByteArray payload;
    QByteArray constellationLineRows;
    QByteArray constellationAnchorGroupRows;
    int constellationLineSchemaVersion = 0;
    std::size_t constellationCount = 0;
};

}  // namespace skygate::ui::internal
