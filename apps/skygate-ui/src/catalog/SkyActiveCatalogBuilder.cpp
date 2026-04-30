#include "SkyActiveCatalogBuilder.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <span>

namespace skygate::ui::internal {
namespace {

std::size_t countDeepSkyObjects(
    const std::span<const skygate::ephemeris::CelestialBody> bodies
)
{
    return static_cast<std::size_t>(std::count_if(
        bodies.begin(),
        bodies.end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject;
        }
    ));
}

}  // namespace

bool SkyActiveCatalogBuildResult::isSuccess() const noexcept
{
    return catalog != nullptr && ephemerisEngine != nullptr;
}

SkyActiveCatalogBuildResult SkyActiveCatalogBuilder::build(
    const SkyActiveCatalogBuildRequest& request
)
{
    SkyActiveCatalogBuildResult result;
    auto activeCatalog = skygate::ephemeris::createCatalogWithCoreSolarSystemBodies(
        request.sourceCatalog
    );
    if (activeCatalog == nullptr) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }

    const skygate::ephemeris::IStarCatalog* deepSkyCatalog = request.deepSkyCatalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledDeepSkyCatalog;
    if (deepSkyCatalog == nullptr && request.useBundledDeepSkyCatalog) {
        bundledDeepSkyCatalog = skygate::ephemeris::createBundledStarCatalog();
        deepSkyCatalog = bundledDeepSkyCatalog.get();
    }

    result.foundDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    if (deepSkyCatalog != nullptr) {
        if (result.foundDeepSkyObjectCount == 0U || request.useBundledDeepSkyCatalog) {
            result.foundDeepSkyObjectCount = countDeepSkyObjects(deepSkyCatalog->bodies());
        }
        activeCatalog = skygate::ephemeris::createCatalogByMergingDeepSkyObjects(
            *activeCatalog,
            *deepSkyCatalog
        );
    }

    if (activeCatalog == nullptr) {
        result.errorText = "Catalog: Failed to merge deep-sky catalog";
        return result;
    }

    const auto bodies = activeCatalog->bodies();
    std::size_t catalogConstellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    for (const auto& body : bodies) {
        if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            ++catalogConstellationCount;
        }
        if (body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
            ++deepSkyObjectCount;
        }
    }

    result.bodyCount = bodies.size();
    result.constellationCount = std::max(
        catalogConstellationCount,
        request.currentConstellationCount
    );
    result.deepSkyObjectCount = deepSkyObjectCount;
    result.ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*activeCatalog);
    if (result.ephemerisEngine == nullptr) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }
    result.statusText =
        QString("Catalog: %1 + %2 (%3 objects, %4 deep sky, %5 constellations)").arg(
            request.sourceLabel,
            request.deepSkySourceLabel,
            QString::number(static_cast<qulonglong>(result.bodyCount)),
            QString::number(static_cast<qulonglong>(result.deepSkyObjectCount)),
            QString::number(static_cast<qulonglong>(result.constellationCount))
        );
    result.catalog = std::move(activeCatalog);
    return result;
}

}  // namespace skygate::ui::internal
