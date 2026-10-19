#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogFieldOrigins.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

// Accumulates deduplicated bodies of one or more sources while keeping logical
// positions stable so an identity index can address each survivor by value.
// A position can be vacated when a later replacing source wins; vacated
// positions are skipped during final assembly. The field origins of a survivor
// are kept beside its body, aligned by position.
struct CatalogMergeAccumulator final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<std::string> sourceIds;
    std::vector<std::vector<std::string>> contributorSourceIds;
    std::vector<CatalogFieldOrigins> fieldOrigins;
    std::vector<bool> isDistant;
    std::vector<std::size_t> domainIndex;
    std::vector<bool> active;

    // Appends a body owned by `sourceId`, together with the origins of the values
    // it carries. The owning source heads the contributor list, followed by the
    // prior contributors in the descending source precedence order the caller
    // established. Prior ids are deduplicated, so a source absorbed through
    // several paths is listed once.
    [[nodiscard]] std::size_t append(
        const BaseCelestialBody& body,
        std::string sourceId,
        CatalogFieldOrigins origins,
        std::vector<std::string> priorContributors = {}
    )
    {
        if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
            distantBodies.push_back(CelestialBodyCatalog::copyDistantBody(body));
            domainIndex.push_back(distantBodies.size() - 1U);
            isDistant.push_back(true);
        } else {
            ownGalaxyBodies.push_back(CelestialBodyCatalog::copyOwnGalaxyBody(body));
            domainIndex.push_back(ownGalaxyBodies.size() - 1U);
            isDistant.push_back(false);
        }
        sourceIds.push_back(std::move(sourceId));

        std::vector<std::string> contributors;
        contributors.reserve(priorContributors.size() + 1U);
        contributors.push_back(sourceIds.back());
        for (std::string& prior : priorContributors) {
            StringUtilities::appendUnique(contributors, std::move(prior));
        }
        contributorSourceIds.push_back(std::move(contributors));
        fieldOrigins.push_back(std::move(origins));
        active.push_back(true);
        return sourceIds.size() - 1U;
    }

    void vacate(const std::size_t position)
    {
        active[position] = false;
    }

    [[nodiscard]] BaseCelestialBody& at(const std::size_t position)
    {
        if (isDistant[position]) {
            return distantBodies[domainIndex[position]];
        }
        return ownGalaxyBodies[domainIndex[position]];
    }

    [[nodiscard]] const BaseCelestialBody& at(const std::size_t position) const
    {
        if (isDistant[position]) {
            return distantBodies[domainIndex[position]];
        }
        return ownGalaxyBodies[domainIndex[position]];
    }
};

}  // namespace skygate::ephemeris
