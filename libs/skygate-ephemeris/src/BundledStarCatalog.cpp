#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include "catalog/BundledCatalogParser.hpp"
#include "catalog/CatalogRowParser.hpp"
#include "catalog/HygCatalogParser.hpp"
#include "catalog/HygGzipCatalogParser.hpp"
#include "catalog/InMemoryStarCatalog.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

std::unique_ptr<IStarCatalog> createCatalogFromBodies(std::vector<CelestialBody> bodies)
{
    if (bodies.empty()) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(std::move(bodies));
}

}  // namespace

std::unique_ptr<IStarCatalog> createStarCatalog(const CatalogSourceRequest& request)
{
    switch (request.type) {
    case CatalogSourceType::Bundled: {
        const BundledCatalogParser parser;
        return createCatalogFromBodies(parser.parse());
    }
    case CatalogSourceType::Rows: {
        const CatalogRowParser parser;
        return createCatalogFromBodies(parser.parse(request.data));
    }
    case CatalogSourceType::HygCsv: {
        const HygCatalogParser parser;
        return createCatalogFromBodies(parser.parse(request.data, request.progressCallback));
    }
    case CatalogSourceType::HygCsvGzip: {
        const HygGzipCatalogParser parser;
        return createCatalogFromBodies(parser.parse(request.data, request.progressCallback));
    }
    }

    return nullptr;
}

std::unique_ptr<IStarCatalog> createStarCatalog(
    const CatalogSourceType type,
    const std::string_view data,
    const HygParseProgressCallback& progressCallback
)
{
    return createStarCatalog(
        CatalogSourceRequest {
            .type = type,
            .data = data,
            .progressCallback = progressCallback
        }
    );
}

std::unique_ptr<IStarCatalog> createBundledStarCatalog()
{
    return createStarCatalog(CatalogSourceType::Bundled);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromRows(const std::string_view catalogRows)
{
    return createStarCatalog(CatalogSourceType::Rows, catalogRows);
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

}  // namespace skygate::ephemeris
