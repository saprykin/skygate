#pragma once

#include "SkyCatalogConstellationStore.hpp"

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QString>

#include <cstddef>
#include <memory>

namespace skygate::ui::internal {

// One configured catalog source held by the runtime.
//
// instanceId is the durable identity used for per-body provenance and for
// lifecycle operations (load, replace, enable, disable, remove). title is
// presentation text and may change independently of the identity. policy
// selects how the catalog participates in composition; enabled toggles
// participation without discarding the loaded catalog.
//
// constellationData is the related constellation dataset owned by this
// instance. It survives the source being disabled and is removed with the
// source; the runtime keeps it when the instance's catalog is replaced and
// replaces it when a stored collection is restored.
struct SkyCatalogSourceRecord final {
    QString instanceId;
    QString title;
    QString version;
    QString url;
    skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bool enabled = true;
    bool bundled = false;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    std::size_t foundObjectCount = 0;
    SkyCatalogConstellationStore constellationData;
};

}  // namespace skygate::ui::internal
