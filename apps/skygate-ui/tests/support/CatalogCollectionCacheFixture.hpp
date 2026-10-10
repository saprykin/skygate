#pragma once

#include "SkySettingsStore.hpp"

#include <QString>

namespace skygate::ui::tests {

class SettingsTestFixture;

// Persisted collection snapshots and cache locations shared by the settings
// storage suites.
class CatalogCollectionCacheFixture final {
public:
    [[nodiscard]] static SkySettingsStore::CatalogCollectionCacheSnapshot sampleCollectionSnapshot();

    static QString prepareCollectionCacheDirectory(const SettingsTestFixture& settings);
};

}  // namespace skygate::ui::tests
