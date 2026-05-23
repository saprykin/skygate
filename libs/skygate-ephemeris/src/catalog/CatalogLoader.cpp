#include "catalog/CatalogLoader.hpp"
#include "catalog/bundled/BundledCatalogParser.hpp"
#include "catalog/CatalogBodyParseResult.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/hyg/HygCatalogParser.hpp"
#include "catalog/ICatalogParser.hpp"
#include "catalog/io/GzipCatalogParser.hpp"
#include "catalog/io/zip/ZipCatalogParser.hpp"
#include "catalog/opengc/OpenNgcCatalogParser.hpp"

#include <algorithm>
#include <memory>
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

    std::vector<CelestialBody> bodies = std::move(parsedBodies.bodies);
    const std::size_t parsedBodyCount = bodies.size();
    if (selectionOptions.isEnabled() && selectionOptions.mode == CatalogSelectionMode::BrightestByVisualMagnitude
        && selectionOptions.maxBodyCount < bodies.size()) {
        std::stable_sort(bodies.begin(), bodies.end(), [](const CelestialBody& lhs, const CelestialBody& rhs) {
            return lhs.visualMagnitude < rhs.visualMagnitude;
        });
        bodies.resize(selectionOptions.maxBodyCount);
    }

    result.diagnostics.parsedBodyCount = parsedBodyCount;
    result.diagnostics.selectedBodyCount = bodies.size();
    result.diagnostics.truncatedBodyCount = parsedBodyCount - bodies.size();
    result.catalog = CatalogFactory::createStarCatalogFromBodies(std::move(bodies));
    if (result.catalog == nullptr) {
        result.errorCode = CatalogLoadResult::ErrorCode::NoBodies;
        result.errorDetail = "Catalog contains no bodies.";
    }

    return result;
}

[[nodiscard]] std::unique_ptr<ICatalogParser> createParser(const CatalogSourceType type)
{
    switch (type) {
    case CatalogSourceType::Bundled:
        return std::make_unique<BundledCatalogParser>();
    case CatalogSourceType::HygCsv:
        return std::make_unique<HygCatalogParser>();
    case CatalogSourceType::HygCsvGzip:
        return std::make_unique<GzipCatalogParser>(std::make_unique<HygCatalogParser>());
    case CatalogSourceType::HygCsvZip:
        return std::make_unique<ZipCatalogParser>();
    case CatalogSourceType::OpenNgcCsv:
        return std::make_unique<OpenNgcCatalogParser>();
    case CatalogSourceType::Unknown:
        return nullptr;
    }

    return nullptr;
}

}  // namespace

CatalogLoadResult CatalogLoader::load(const CatalogSourceRequest& request)
{
    const auto parser = createParser(request.type);
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
