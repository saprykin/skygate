#include "CatalogLoader.hpp"
#include "CatalogBodyParseResult.hpp"
#include "CatalogFactory.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "catalog/normalize/CatalogSnapshotValidator.hpp"

#include <QLoggingCategory>
#include <QString>

#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogLoadLog, "skygate.catalog.load")

CatalogLoadResult finalizeCatalogLoad(CatalogBodyParseResult parsedBodies)
{
    CatalogLoadResult result;
    result.errorCode = parsedBodies.errorCode;
    result.errorDetail = std::move(parsedBodies.errorDetail);
    result.diagnostics = parsedBodies.diagnostics;
    if (!parsedBodies.isSuccess()) {
        return result;
    }

    std::vector<OwnGalaxyCelestialBody> bodies = std::move(parsedBodies.bodies);
    std::vector<DistantCelestialBody> distantBodies = std::move(parsedBodies.distantBodies);
    const std::size_t parsedBodyCount = bodies.size() + distantBodies.size();

    const CatalogSnapshotValidator::Report report =
        CatalogSnapshotValidator::validate(bodies, distantBodies, parsedBodies.orderedBodyIndexes);
    if (!report.ok) {
        result.errorCode = CatalogLoadResult::ErrorCode::NoBodies;
        result.errorDetail = report.errorDetail;
        qCWarning(skygateCatalogLoadLog).noquote()
            << "Catalog snapshot validation failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    result.catalog = CatalogFactory::createStarCatalogFromBodies(
        std::move(bodies), std::move(distantBodies), std::move(parsedBodies.orderedBodyIndexes)
    );

    result.diagnostics.parsedBodyCount = parsedBodyCount;
    result.diagnostics.selectedBodyCount = result.catalog != nullptr ? result.catalog->bodies().size() : 0U;
    result.diagnostics.truncatedBodyCount = parsedBodyCount - result.diagnostics.selectedBodyCount;
    if (result.catalog == nullptr) {
        result.errorCode = CatalogLoadResult::ErrorCode::NoBodies;
        result.errorDetail = "Catalog contains no bodies.";
    }

    return result;
}

}  // namespace

CatalogLoadResult CatalogLoader::load(const CatalogSourceRequest& request)
{
    const auto parser = CatalogSchemaRegistry::createParser(request.type);
    if (parser == nullptr) {
        CatalogLoadResult result;
        result.errorCode = CatalogLoadResult::ErrorCode::UnsupportedFormat;
        result.errorDetail = "Catalog source type is not supported.";
        return result;
    }

    return finalizeCatalogLoad(parser->parse(request.data, request.progressCallback));
}

CatalogLoadResult CatalogLoader::load(
    const CatalogSourceType type, const std::string_view data, const CatalogParseProgressCallback& progressCallback
)
{
    return load(CatalogSourceRequest{.type = type, .data = data, .progressCallback = progressCallback});
}

}  // namespace skygate::ephemeris
