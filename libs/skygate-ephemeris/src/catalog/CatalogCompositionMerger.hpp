#pragma once

#include "CelestialBodyCatalog.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogCompositionMergeResult.hpp"
#include "catalog/CatalogCompositionRequest.hpp"
#include "catalog/CatalogFieldOrigins.hpp"

#include <QtGlobal>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

// Composition merge driver for catalog source collections.
//
// The merge contract is documented on CatalogIdentityAbsorption. This class owns
// one merge run: the configured source precedence, the survivor accumulator the
// merge stages fill, the identity decision, and the assembly of the result.
class CatalogCompositionMerger final {
public:
    // Accumulates deduplicated bodies of one or more sources while keeping logical
    // positions stable so an identity index can address each survivor by value.
    // A position can be vacated when a later replacing source wins; vacated
    // positions are skipped during final assembly. The field origins of a survivor
    // are kept beside its body, aligned by position.
    struct MergeAccumulator final {
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

    // Configured precedence of every source in a composition collection, keyed by
    // source instance id. A source's rank is its position in the configured
    // collection: a later source outranks every earlier source, exactly like a
    // replacement winner outranks the survivor it absorbs. The rank is never
    // derived from the accumulator position of a survivor, because replacement and
    // bridge absorption vacate accumulator positions and append the winner at a
    // new position, so position order stops matching source precedence after the
    // first replacement.
    class SourcePrecedence final {
    public:
        void add(const std::string_view sourceId, const std::size_t rank)
        {
            m_ranks.insert_or_assign(std::string{sourceId}, rank);
        }

        [[nodiscard]] std::size_t rankOf(const std::string_view sourceId) const
        {
            const auto found = m_ranks.find(std::string{sourceId});
            Q_ASSERT(found != m_ranks.end());
            return found != m_ranks.end() ? found->second : 0U;
        }

        [[nodiscard]] bool outranks(const std::string_view lhsSourceId, const std::string_view rhsSourceId) const
        {
            return rankOf(lhsSourceId) > rankOf(rhsSourceId);
        }

    private:
        std::unordered_map<std::string, std::size_t> m_ranks;
    };

    [[nodiscard]] static CatalogCompositionMergeResult mergeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
