#include "CatalogLoader.hpp"
#include "CatalogBodyParseResult.hpp"
#include "CatalogFactory.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "catalog/normalize/CatalogBodyNormalization.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

CatalogLoadResult
finalizeCatalogLoad(CatalogBodyParseResult parsedBodies, const CatalogSelectionOptions& selectionOptions)
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
    if (selectionOptions.isEnabled() && selectionOptions.mode == CatalogSelectionMode::BrightestByVisualMagnitude
        && selectionOptions.maxBodyCount < parsedBodyCount) {
        CatalogBodyNormalization::apply(bodies);
        CelestialBodyCatalog sourceCatalog(
            std::move(bodies), std::move(distantBodies), std::move(parsedBodies.orderedBodyIndexes)
        );
        std::vector<const BaseCelestialBody*> selectedBodies;
        selectedBodies.reserve(sourceCatalog.size());
        for (const BaseCelestialBody* body : sourceCatalog.bodies()) {
            selectedBodies.push_back(body);
        }
        std::stable_sort(
            selectedBodies.begin(),
            selectedBodies.end(),
            [](const BaseCelestialBody* lhs, const BaseCelestialBody* rhs) {
                return lhs->visualMagnitude < rhs->visualMagnitude;
            }
        );
        selectedBodies.resize(selectionOptions.maxBodyCount);
        result.catalog = CatalogFactory::createStarCatalogFromCatalog(CelestialBodyCatalog(selectedBodies));
    } else {
        result.catalog = CatalogFactory::createStarCatalogFromBodies(
            std::move(bodies), std::move(distantBodies), std::move(parsedBodies.orderedBodyIndexes)
        );
    }

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

    return finalizeCatalogLoad(parser->parse(request.data, request.progressCallback), request.selectionOptions);
}

CatalogLoadResult CatalogLoader::load(
    const CatalogSourceType type,
    const std::string_view data,
    const CatalogParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
)
{
    return load(
        CatalogSourceRequest{
            .type = type, .data = data, .progressCallback = progressCallback, .selectionOptions = selectionOptions
        }
    );
}

}  // namespace skygate::ephemeris
