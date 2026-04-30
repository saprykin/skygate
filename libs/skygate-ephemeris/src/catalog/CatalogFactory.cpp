#include "skygate/ephemeris/CatalogFactory.hpp"

#include "catalog/CatalogBodyNormalization.hpp"
#include "catalog/InMemoryStarCatalog.hpp"

#include <memory>
#include <utility>

namespace skygate::ephemeris {

std::unique_ptr<IStarCatalog> createStarCatalog(const CatalogSourceRequest& request)
{
    CatalogLoadResult result = loadStarCatalog(request);
    return std::move(result.catalog);
}

std::unique_ptr<IStarCatalog> createStarCatalog(
    const CatalogSourceType type,
    const std::string_view data,
    const HygParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
)
{
    CatalogLoadResult result = loadStarCatalog(type, data, progressCallback, selectionOptions);
    return std::move(result.catalog);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromBodies(std::vector<CelestialBody> bodies)
{
    if (bodies.empty()) {
        return nullptr;
    }

    CatalogBodyNormalization::apply(bodies);
    return std::make_unique<InMemoryStarCatalog>(std::move(bodies));
}

std::unique_ptr<IStarCatalog> createBundledStarCatalog()
{
    return createStarCatalog(CatalogSourceType::Bundled);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsv(const std::string_view csvData)
{
    return createStarCatalogFromHygCsv(csvData, {});
}

std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsv(
    const std::string_view csvData,
    const HygParseProgressCallback& progressCallback
)
{
    return createStarCatalog(CatalogSourceType::HygCsv, csvData, progressCallback);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsvGzip(const std::string_view gzipData)
{
    return createStarCatalogFromHygCsvGzip(gzipData, {});
}

std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsvGzip(
    const std::string_view gzipData,
    const HygParseProgressCallback& progressCallback
)
{
    return createStarCatalog(CatalogSourceType::HygCsvGzip, gzipData, progressCallback);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromOpenNgcCsv(const std::string_view csvData)
{
    return createStarCatalogFromOpenNgcCsv(csvData, {});
}

std::unique_ptr<IStarCatalog> createStarCatalogFromOpenNgcCsv(
    const std::string_view csvData,
    const HygParseProgressCallback& progressCallback
)
{
    return createStarCatalog(CatalogSourceType::OpenNgcCsv, csvData, progressCallback);
}

}  // namespace skygate::ephemeris
