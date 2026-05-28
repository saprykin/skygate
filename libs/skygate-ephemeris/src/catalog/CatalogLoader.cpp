#include "CatalogLoader.hpp"
#include "CatalogBodyParseResult.hpp"
#include "CatalogFactory.hpp"
#include "ICatalogParser.hpp"
#include "catalog/bundled/BundledCatalogParser.hpp"
#include "catalog/hyg/HygCatalogParser.hpp"
#include "catalog/io/GzipCatalogParser.hpp"
#include "catalog/normalize/CatalogBodyNormalization.hpp"
#include "catalog/opengc/OpenNgcCatalogParser.hpp"
#include "catalog/io/zip/ZipCatalogParser.hpp"

#include <algorithm>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] std::unique_ptr<IStarCatalog>
createCatalogFromSelectedBodies(const std::span<const BaseCelestialBody* const> selectedBodies)
{
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CelestialBodyCatalog::OrderEntry> orderedBodyIndexes;
    ownGalaxyBodies.reserve(selectedBodies.size());
    distantBodies.reserve(selectedBodies.size());
    orderedBodyIndexes.reserve(selectedBodies.size());

    for (const BaseCelestialBody* body : selectedBodies) {
        if (body == nullptr) {
            continue;
        }

        if (body->kind == BaseCelestialBody::Kind::DeepSkyObject) {
            DistantCelestialBody distantBody;
            distantBody.id = body->id;
            distantBody.displayName = body->displayName;
            distantBody.kind = body->kind;
            distantBody.visualMagnitude = body->visualMagnitude;
            distantBody.fixedEquatorial = body->fixedEquatorialValue();
            distantBody.deepSkyObject = body->deepSkyObjectValue();
            orderedBodyIndexes.push_back(
                CelestialBodyCatalog::OrderEntry{
                    .domain = CelestialBodyCatalog::BodyDomain::Distant,
                    .bodyIndex = distantBodies.size(),
                }
            );
            distantBodies.push_back(std::move(distantBody));
        } else {
            OwnGalaxyCelestialBody ownGalaxyBody;
            ownGalaxyBody.id = body->id;
            ownGalaxyBody.displayName = body->displayName;
            ownGalaxyBody.kind = body->kind;
            ownGalaxyBody.visualMagnitude = body->visualMagnitude;
            ownGalaxyBody.fixedEquatorial = body->fixedEquatorialValue();
            ownGalaxyBody.starAstrometry = body->starAstrometryValue();
            orderedBodyIndexes.push_back(
                CelestialBodyCatalog::OrderEntry{
                    .domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                    .bodyIndex = ownGalaxyBodies.size(),
                }
            );
            ownGalaxyBodies.push_back(std::move(ownGalaxyBody));
        }
    }

    return CatalogFactory::createStarCatalogFromBodies(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(orderedBodyIndexes)
    );
}

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
        result.catalog = createCatalogFromSelectedBodies(selectedBodies);
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
