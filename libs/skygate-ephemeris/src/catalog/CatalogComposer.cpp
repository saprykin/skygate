#include "CatalogComposer.hpp"

#include "CatalogCompositionMerger.hpp"
#include "CatalogFactory.hpp"
#include "CatalogIdentity.hpp"
#include "StringUtilities.hpp"

#include <QLoggingCategory>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCompositionLog, "skygate.catalog.composition")

void assignCompositionCounts(
    CatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::size_t currentConstellationCount
)
{
    std::size_t catalogConstellationCount = 0;
    for (const BaseCelestialBody* body : bodies) {
        if (body == nullptr) {
            continue;
        }
        switch (body->kind) {
        case BaseCelestialBody::Kind::Star:
            ++result.starCount;
            break;
        case BaseCelestialBody::Kind::Planet:
            ++result.planetCount;
            break;
        case BaseCelestialBody::Kind::Moon:
            ++result.moonCount;
            break;
        case BaseCelestialBody::Kind::Sun:
            ++result.sunCount;
            break;
        case BaseCelestialBody::Kind::Constellation:
            ++catalogConstellationCount;
            break;
        case BaseCelestialBody::Kind::DeepSkyObject:
            ++result.deepSkyObjectCount;
            break;
        }
    }

    result.bodyCount = bodies.size();
    result.constellationCount = std::max(catalogConstellationCount, currentConstellationCount);

    result.sourceOrder.clear();
    result.sourceRowCounts.clear();
    result.sourceOrder.reserve(result.sourceIds.size());
    result.sourceRowCounts.reserve(result.sourceIds.size());
    for (const std::string& sourceId : result.sourceIds) {
        const auto existing = std::find(result.sourceOrder.begin(), result.sourceOrder.end(), sourceId);
        if (existing == result.sourceOrder.end()) {
            result.sourceOrder.push_back(sourceId);
            result.sourceRowCounts.push_back(1U);
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(std::distance(result.sourceOrder.begin(), existing));
        ++result.sourceRowCounts[index];
    }
}

// Returns a diagnostic when the collection violates the documented source
// identity contract: every source instance must have a non-empty identity that
// is unique within the collection. Identities are compared through the same
// normalization used for composed identity keys (trimmed, ASCII case folded),
// so two entries that would still resolve to the same key are rejected as
// duplicates.
[[nodiscard]] std::optional<std::string> invalidSourceIdentityDetail(const CatalogCompositionRequest& request)
{
    std::vector<std::string> seenKeys;
    seenKeys.reserve(request.sources.size());
    for (std::size_t index = 0; index < request.sources.size(); ++index) {
        const std::string& sourceId = request.sources[index].sourceId;
        const std::string identityKey = StringUtilities::normalizedLookupKey(sourceId);
        if (identityKey.empty()) {
            return "catalog composition rejected source " + std::to_string(index) + ": the source identity is empty.";
        }
        if (std::find(seenKeys.begin(), seenKeys.end(), identityKey) != seenKeys.end()) {
            return "catalog composition rejected source " + std::to_string(index) + ": source identity '" + sourceId
                   + "' duplicates another source in the collection.";
        }
        seenKeys.push_back(identityKey);
    }
    return std::nullopt;
}

std::size_t countDeepSkyObjects(const IStarCatalog* catalog)
{
    return catalog != nullptr ? CatalogIdentity::countDeepSkyObjects(catalog->bodies()) : 0U;
}

}  // namespace

CatalogCompositionResult CatalogComposer::composeCollection(const CatalogCompositionRequest& request)
{
    CatalogCompositionResult result;
    if (const std::optional<std::string> invalidSourceDetail = invalidSourceIdentityDetail(request);
        invalidSourceDetail.has_value()) {
        result.errorCode = CatalogCompositionResult::ErrorCode::InvalidSourceIdentity;
        result.errorDetail = *invalidSourceDetail;
        qCWarning(skygateCatalogCompositionLog).noquote() << QString::fromStdString(*invalidSourceDetail);
        return result;
    }

    CatalogCompositionMergeResult merged = CatalogCompositionMerger::mergeCollection(request);
    result.catalog = CatalogFactory::createStarCatalogFromBodies(
        std::move(merged.ownGalaxyBodies), std::move(merged.distantBodies), std::move(merged.orderedBodyIndexes)
    );
    if (result.catalog == nullptr) {
        result.sourceIds.clear();
        return result;
    }

    result.sourceIds = std::move(merged.sourceIds);
    result.contributorSourceIds = std::move(merged.contributorSourceIds);
    assignCompositionCounts(result, result.catalog->bodies(), request.currentConstellationCount);

    result.foundDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    if (result.foundDeepSkyObjectCount == 0U) {
        for (const CatalogCompositionSourceEntry& source : request.sources) {
            if (!source.enabled || source.catalog == nullptr) {
                continue;
            }
            if (source.policy == CatalogCompositionPolicy::DeepSkyOnly
                || source.policy == CatalogCompositionPolicy::DeepSkyFallback) {
                result.foundDeepSkyObjectCount += countDeepSkyObjects(source.catalog);
            }
        }
    }
    return result;
}

}  // namespace skygate::ephemeris
