#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include "catalog/BundledCatalogParser.hpp"
#include "catalog/CatalogBodyNormalization.hpp"
#include "catalog/CatalogRowParser.hpp"
#include "catalog/HygCatalogParser.hpp"
#include "catalog/HygGzipCatalogParser.hpp"
#include "catalog/InMemoryStarCatalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

std::unique_ptr<IStarCatalog> createCatalogFromBodiesInternal(std::vector<CelestialBody> bodies)
{
    if (bodies.empty()) {
        return nullptr;
    }

    CatalogBodyNormalization::apply(bodies);
    return std::make_unique<InMemoryStarCatalog>(std::move(bodies));
}

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
    result.catalog = createCatalogFromBodiesInternal(std::move(bodies));
    if (result.catalog == nullptr) {
        result.errorCode = CatalogLoadErrorCode::NoBodies;
        result.errorDetail = "Catalog contains no bodies.";
    }

    return result;
}

std::string toLowerId(const std::string& value)
{
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lower;
}

bool isSunOrMoonType(const CelestialBodyType type)
{
    return type == CelestialBodyType::Sun || type == CelestialBodyType::Moon;
}

bool isReferenceLineStarId(const std::string& id)
{
    constexpr std::array<std::string_view, 8> kReferenceLineStarIds = {{
        "sirius",
        "canopus",
        "arcturus",
        "vega",
        "capella",
        "rigel",
        "procyon",
        "betelgeuse",
    }};

    const std::string loweredId = toLowerId(id);
    return std::any_of(kReferenceLineStarIds.begin(), kReferenceLineStarIds.end(), [&loweredId](const auto value) {
        return loweredId == value;
    });
}

bool containsBodyIdCaseInsensitive(
    const std::vector<CelestialBody>& bodies,
    const std::string& id
)
{
    const std::string loweredId = toLowerId(id);
    return std::any_of(bodies.begin(), bodies.end(), [&loweredId](const auto& body) {
        return toLowerId(body.id) == loweredId;
    });
}

}  // namespace

CatalogLoadResult loadStarCatalog(const CatalogSourceRequest& request)
{
    switch (request.type) {
    case CatalogSourceType::Bundled: {
        const BundledCatalogParser parser;
        return finalizeCatalogLoad(parser.parse(), request.selectionOptions);
    }
    case CatalogSourceType::Rows: {
        const CatalogRowParser parser;
        return finalizeCatalogLoad(parser.parse(request.data), request.selectionOptions);
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
    return createCatalogFromBodiesInternal(std::move(bodies));
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

std::unique_ptr<IStarCatalog> createCatalogWithCoreSolarSystemBodies(const IStarCatalog& catalog)
{
    return createCatalogWithCoreSolarSystemBodies(catalog.bodies());
}

std::unique_ptr<IStarCatalog> createCatalogWithCoreSolarSystemBodies(
    const std::span<const CelestialBody> bodies
)
{
    std::vector<CelestialBody> mergedBodies(bodies.begin(), bodies.end());
    const std::size_t starCount = static_cast<std::size_t>(std::count_if(
        mergedBodies.begin(),
        mergedBodies.end(),
        [](const auto& body) {
            return body.type == CelestialBodyType::Star;
        }
    ));

    std::unique_ptr<IStarCatalog> bundledCatalog = createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return createStarCatalogFromBodies(std::move(mergedBodies));
    }

    for (const auto& body : bundledCatalog->bodies()) {
        const bool isReferenceLineStar = body.type == CelestialBodyType::Star
            && isReferenceLineStarId(body.id)
            && starCount == 0U;
        if (
            !isSunOrMoonType(body.type)
            && body.type != CelestialBodyType::Planet
            && !isReferenceLineStar
        ) {
            continue;
        }

        if (containsBodyIdCaseInsensitive(mergedBodies, body.id)) {
            continue;
        }

        mergedBodies.push_back(body);
    }

    return createStarCatalogFromBodies(std::move(mergedBodies));
}

}  // namespace skygate::ephemeris
