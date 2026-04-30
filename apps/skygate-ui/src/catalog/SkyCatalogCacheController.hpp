#pragma once

#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace skygate::ui::internal {

struct SkyCatalogCacheRestoreResult final {
    bool restored = false;
    bool savedCatalogUnreadable = false;
    QString statusText;
    QByteArray catalogPayload;
    QByteArray deepSkyCatalogPayload;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> deepSkyCatalog;
    QString sourceLabel;
    QString deepSkySourceLabel;
    std::size_t deepSkyObjectCount = 0;
    std::vector<skygate::ephemeris::ConstellationLineRef> constellationLineRefs;
    std::vector<skygate::ephemeris::ConstellationLabelRef> constellationLabelRefs;
    std::optional<std::size_t> constellationCount;
    bool resetConstellationLineRefs = false;
};

struct SkyCatalogCachePersistRequest final {
    QString sourceLabel;
    QString deepSkySourceLabel;
    QByteArray catalogPayload;
    QByteArray deepSkyCatalogPayload;
    std::vector<skygate::ephemeris::ConstellationLineRef> constellationLineRefs;
    std::vector<skygate::ephemeris::ConstellationLabelRef> constellationLabelRefs;
    std::size_t constellationCount = 0;
};

class SkyCatalogCacheController final {
public:
    explicit SkyCatalogCacheController(SkySettingsStore* settingsStore);

    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool clearDeepSkyCatalogCache() const;
    [[nodiscard]] SkyCatalogCacheRestoreResult restore(
        int catalogPresetIndex,
        int deepSkyCatalogPresetIndex
    ) const;
    void persist(const SkyCatalogCachePersistRequest& request) const;

private:
    SkySettingsStore* m_settingsStore = nullptr;
};

}  // namespace skygate::ui::internal
