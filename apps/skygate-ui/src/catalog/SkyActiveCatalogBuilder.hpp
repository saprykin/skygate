#pragma once

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <QString>

#include <cstddef>
#include <memory>

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

struct SkyActiveCatalogBuildResult final {
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine;
    QString statusText;
    QString errorText;
    std::size_t bodyCount = 0;
    std::size_t constellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    std::size_t foundDeepSkyObjectCount = 0;

    [[nodiscard]] bool isSuccess() const noexcept;
};

class SkyActiveCatalogBuilder final {
public:
    [[nodiscard]] static SkyActiveCatalogBuildResult build(
        const SkyActiveCatalogBuildRequest& request
    );
};

}  // namespace skygate::ui::internal
