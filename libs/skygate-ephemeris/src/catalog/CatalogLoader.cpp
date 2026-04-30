#include "skygate/ephemeris/CatalogLoader.hpp"

#include "catalog/BundledCatalogParser.hpp"
#include "catalog/CatalogBodyParseResult.hpp"
#include "catalog/HygCatalogParser.hpp"
#include "catalog/HygGzipCatalogParser.hpp"
#include "catalog/OpenNgcCatalogParser.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

CatalogLoadResult finalizeCatalogLoad(
    CatalogBodyParseResult parsedBodies,
    const CatalogSelectionOptions& selectionOptions
)
{
    CatalogLoadResult result;
    result.errorCode = parsedBodies.errorCode;
    result.errorDetail = std::move(parsedBodies.errorDetail);
    result.diagnostics = parsedBodies.diagnostics;
    if (!parsedBodies.isSuccess()) {
        return result;
    }

    std::vector<CelestialBody> bodies = std::move(parsedBodies.bodies);
    const std::size_t parsedBodyCount = bodies.size();
    if (
        selectionOptions.isEnabled()
        && selectionOptions.mode == CatalogSelectionMode::BrightestByVisualMagnitude
        && selectionOptions.maxBodyCount < bodies.size()
    ) {
        std::stable_sort(bodies.begin(), bodies.end(), [](const CelestialBody& lhs, const CelestialBody& rhs) {
            return lhs.visualMagnitude < rhs.visualMagnitude;
        });
        bodies.resize(selectionOptions.maxBodyCount);
    }

    result.diagnostics.parsedBodyCount = parsedBodyCount;
    result.diagnostics.selectedBodyCount = bodies.size();
    result.diagnostics.truncatedBodyCount = parsedBodyCount - bodies.size();
    result.catalog = createStarCatalogFromBodies(std::move(bodies));
    if (result.catalog == nullptr) {
        result.errorCode = CatalogLoadErrorCode::NoBodies;
        result.errorDetail = "Catalog contains no bodies.";
    }

    return result;
}

}  // namespace

CatalogLoadResult loadStarCatalog(const CatalogSourceRequest& request)
{
    switch (request.type) {
    case CatalogSourceType::Bundled: {
        const BundledCatalogParser parser;
        return finalizeCatalogLoad(parser.parse(), request.selectionOptions);
    }
    case CatalogSourceType::HygCsv: {
        const HygCatalogParser parser;
        return finalizeCatalogLoad(
            parser.parse(request.data, request.progressCallback),
            request.selectionOptions
        );
    }
    case CatalogSourceType::HygCsvGzip: {
        const HygGzipCatalogParser parser;
        return finalizeCatalogLoad(
            parser.parse(request.data, request.progressCallback),
            request.selectionOptions
        );
    }
    case CatalogSourceType::OpenNgcCsv: {
        const OpenNgcCatalogParser parser;
        return finalizeCatalogLoad(
            parser.parse(request.data, request.progressCallback),
            request.selectionOptions
        );
    }
    }

    CatalogLoadResult result;
    result.errorCode = CatalogLoadErrorCode::UnsupportedFormat;
    result.errorDetail = "Catalog source type is not supported.";
    return result;
}

CatalogLoadResult loadStarCatalog(
    const CatalogSourceType type,
    const std::string_view data,
    const HygParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
)
{
    return loadStarCatalog(
        CatalogSourceRequest {
            .type = type,
            .data = data,
            .progressCallback = progressCallback,
            .selectionOptions = selectionOptions
        }
    );
}

}  // namespace skygate::ephemeris
