#pragma once

#include "catalog/IStarCatalog.hpp"

#include <QHash>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <memory>
#include <vector>

namespace skygate::ui::internal {

struct SkyActiveCatalogBuildRequest final {
    const skygate::ephemeris::IStarCatalog& sourceCatalog;
    const skygate::ephemeris::IStarCatalog* deepSkyCatalog = nullptr;
    bool useBundledDeepSkyCatalog = false;
    std::size_t currentConstellationCount = 0;
    std::size_t knownDeepSkyObjectCount = 0;
    QString sourceLabel;
    QString deepSkySourceLabel;
};

// Two-slot legacy build result. Provenance is expressed with stable source
// instance IDs rather than byte-sized label indexes: sourceIds and
// contributorSourceIds are parallel to the composed catalog's bodies, and
// sourceTitles maps each stable ID to its presentation title.
struct SkyActiveCatalogBuildResult final {
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    QString statusText;
    QString errorText;
    std::size_t bodyCount = 0;
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;
    std::vector<QString> sourceIds;
    std::vector<QStringList> contributorSourceIds;
    QHash<QString, QString> sourceTitles;

    [[nodiscard]] bool isSuccess() const noexcept;
};

class SkyActiveCatalogBuilder final {
public:
    [[nodiscard]] static SkyActiveCatalogBuildResult build(const SkyActiveCatalogBuildRequest& request);
};

}  // namespace skygate::ui::internal
