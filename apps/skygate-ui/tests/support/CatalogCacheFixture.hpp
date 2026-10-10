#pragma once

#include "SkySettingsStore.hpp"
#include "catalog/SkyCatalogCacheController.hpp"

#include <QString>

#include <memory>
#include <string>

namespace skygate::ui::tests {

// Persisted collection fixtures shared by the catalog cache controller
// suites.
class CatalogCacheFixture final {
public:
    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog>
    makeStarCatalog(const std::string& id, const std::string& name);

    // A persisted collection in the current record format: two sources whose
    // raw payloads must be reparsed because they carry no binary sidecars.
    [[nodiscard]] static SkySettingsStore::CatalogCollectionCacheSnapshot makeCollectionSnapshot();

    [[nodiscard]] static QString firstBodyId(const skygate::ephemeris::IStarCatalog& catalog);

    [[nodiscard]] static skygate::ui::internal::SkyCatalogCollectionPersistRequest
    persistRequestFromRestoreResult(const skygate::ui::internal::SkyCatalogCollectionRestoreResult& result);
};

}  // namespace skygate::ui::tests
