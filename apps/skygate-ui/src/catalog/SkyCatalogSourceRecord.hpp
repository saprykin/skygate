#pragma once

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
struct SkyCatalogSourceRecord final {
    QString instanceId;
    QString title;
    QString version;
    skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bool enabled = true;
    bool bundled = false;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    std::size_t foundObjectCount = 0;
};

}  // namespace skygate::ui::internal
