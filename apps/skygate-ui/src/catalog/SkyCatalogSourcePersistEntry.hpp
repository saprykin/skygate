#pragma once

#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogSourceCacheRecord.hpp"
#include "catalog/SkyCatalogSourceRecord.hpp"

namespace skygate::ui::internal {

// One catalog source on its way into the durable collection: the record that
// will be stored and the catalog its binary sidecar is serialized from. The
// catalog stays borrowed, and the caller that knows the source's operation
// state supplies the accepted payload before handing the entry over.
struct SkyCatalogSourcePersistEntry final {
    SkyCatalogSourceCacheRecord record;
    const skygate::ephemeris::IStarCatalog* catalog = nullptr;

    // The entry the persist path starts from for a source the runtime holds.
    // Only the configuration the runtime record owns is copied: accepted
    // facts, the payload, and the borrowed catalog pointer are decided by the
    // caller that knows the source's operation state.
    [[nodiscard]] static SkyCatalogSourcePersistEntry fromRuntimeRecord(const SkyCatalogSourceRecord& source);
};

}  // namespace skygate::ui::internal
