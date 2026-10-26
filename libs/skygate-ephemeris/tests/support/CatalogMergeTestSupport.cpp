#include "CatalogMergeTestSupport.hpp"

#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogCompositionRequest.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "catalog/IStarCatalog.hpp"
#include "time/EpochCodec.hpp"
#include "time/TimeScale.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris::tests {

using skygate::core::TimeScale;

OwnGalaxyCelestialBody CatalogMergeTestSupport::makeStar(
    std::string id,
    std::string displayName,
    std::vector<CatalogIdentifier> identifiers,
    std::vector<std::string> aliases,
    std::optional<double> rightAscensionHours,
    std::optional<double> declinationDeg,
    double visualMagnitude
)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::Star;
    body.visualMagnitude = visualMagnitude;
    body.identity.externalIdentifiers = std::move(identifiers);
    body.identity.aliases = std::move(aliases);
    if (rightAscensionHours.has_value() && declinationDeg.has_value()) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = *rightAscensionHours, .declinationDeg = *declinationDeg
        };
    }
    return body;
}

DistantCelestialBody CatalogMergeTestSupport::makeDeepSkyObject(
    std::string id,
    std::string displayName,
    std::vector<std::string> aliases,
    std::vector<CatalogIdentifier> identifiers,
    std::optional<double> rightAscensionHours,
    std::optional<double> declinationDeg,
    std::optional<double> majorAxisArcmin,
    double visualMagnitude
)
{
    DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = visualMagnitude;
    body.identity.externalIdentifiers = std::move(identifiers);
    body.identity.aliases = aliases;
    if (rightAscensionHours.has_value() && declinationDeg.has_value()) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = *rightAscensionHours, .declinationDeg = *declinationDeg
        };
    }
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = std::move(aliases),
        .majorAxisArcmin = majorAxisArcmin,
    };
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogMergeTestSupport::createCatalog(
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies
)
{
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.reserve(ownGalaxyBodies.size() + distantBodies.size());
    for (std::size_t index = 0; index < ownGalaxyBodies.size(); ++index) {
        order.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                .bodyIndex = index,
            }
        );
    }
    for (std::size_t index = 0; index < distantBodies.size(); ++index) {
        order.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant,
                .bodyIndex = index,
            }
        );
    }
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(order)
    );
}

CatalogCompositionResult CatalogMergeTestSupport::composePrimary(const skygate::ephemeris::IStarCatalog& source)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = &source, .policy = CatalogCompositionPolicy::Merge},
    };
    return CatalogComposer::composeCollection(request);
}

CatalogCompositionResult CatalogMergeTestSupport::composePrimaryWithDeepSky(
    const skygate::ephemeris::IStarCatalog& source, const skygate::ephemeris::IStarCatalog& deepSky
)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "primary", .enabled = true, .catalog = &source, .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "deep-sky", .enabled = true, .catalog = &deepSky, .policy = CatalogCompositionPolicy::DeepSkyOnly},
    };
    return CatalogComposer::composeCollection(request);
}

// Composes the two sources in collection order; the second source owns the
// higher merge precedence.
CatalogCompositionResult CatalogMergeTestSupport::composeInOrder(
    const skygate::ephemeris::IStarCatalog& first, const skygate::ephemeris::IStarCatalog& second
)
{
    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "first", .enabled = true, .catalog = &first, .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "second", .enabled = true, .catalog = &second, .policy = CatalogCompositionPolicy::Merge},
    };
    return CatalogComposer::composeCollection(request);
}

// Composes every catalog in collection order as an enabled Merge source with a
// position-derived source identity, so multi-stage replacement and bridge
// chains stay readable.
CatalogCompositionResult CatalogMergeTestSupport::composeAll(const std::vector<const IStarCatalog*>& catalogs)
{
    CatalogCompositionRequest request;
    request.sources.reserve(catalogs.size());
    for (std::size_t index = 0; index < catalogs.size(); ++index) {
        request.sources.push_back(
            {.sourceId = "source-" + std::to_string(index),
             .enabled = true,
             .catalog = catalogs[index],
             .policy = CatalogCompositionPolicy::Merge}
        );
    }
    return CatalogComposer::composeCollection(request);
}

skygate::core::AstronomicalEpoch CatalogMergeTestSupport::j2000Epoch()
{
    return skygate::core::EpochCodec::j2000Epoch(TimeScale::Tt);
}

// Gives a star row a reference position and reference epoch that agree with
// its fixed position, so both coordinate representations describe one model.
OwnGalaxyCelestialBody CatalogMergeTestSupport::withReferenceModel(OwnGalaxyCelestialBody body)
{
    body.starAstrometry = CatalogStarAstrometry{
        .referenceEquatorial = *body.fixedEquatorial,
        .referenceEpoch = j2000Epoch(),
    };
    return body;
}

const BaseCelestialBody*
CatalogMergeTestSupport::findBodyById(const std::span<const BaseCelestialBody* const> bodies, const std::string_view id)
{
    const auto it = std::find_if(bodies.begin(), bodies.end(), [id](const BaseCelestialBody* body) {
        return body != nullptr && body->id == id;
    });
    return it == bodies.end() ? nullptr : *it;
}

std::size_t CatalogMergeTestSupport::countBodiesById(
    const std::span<const BaseCelestialBody* const> bodies, const std::string_view id
)
{
    return static_cast<std::size_t>(std::count_if(bodies.begin(), bodies.end(), [id](const BaseCelestialBody* body) {
        return body != nullptr && body->id == id;
    }));
}

bool CatalogMergeTestSupport::hasIdentifier(
    const BaseCelestialBody& body, const std::string_view namespaceName, const std::string_view value
)
{
    return std::any_of(
        body.identity.externalIdentifiers.begin(),
        body.identity.externalIdentifiers.end(),
        [&](const CatalogIdentifier& identifier) {
            return identifier.namespaceName == namespaceName && identifier.value == value;
        }
    );
}

bool CatalogMergeTestSupport::hasAlias(const std::vector<std::string>& aliases, const std::string_view alias)
{
    return std::find(aliases.begin(), aliases.end(), alias) != aliases.end();
}

bool CatalogMergeTestSupport::hasRetainedCanonicalId(const BaseCelestialBody& body, const std::string_view retainedId)
{
    return std::find(body.identity.retainedCanonicalIds.begin(), body.identity.retainedCanonicalIds.end(), retainedId)
           != body.identity.retainedCanonicalIds.end();
}

// Sorted identifier keys, so two survivors can be compared independently of the
// order in which the composition accumulated them.
std::vector<std::string> CatalogMergeTestSupport::sortedIdentifierKeys(const BaseCelestialBody& body)
{
    std::vector<std::string> keys;
    keys.reserve(body.identity.externalIdentifiers.size());
    for (const CatalogIdentifier& identifier : body.identity.externalIdentifiers) {
        keys.push_back(identifier.key());
    }
    std::ranges::sort(keys);
    return keys;
}

std::vector<std::string> CatalogMergeTestSupport::sortedRetainedCanonicalIds(const BaseCelestialBody& body)
{
    std::vector<std::string> retainedIds = body.identity.retainedCanonicalIds;
    std::ranges::sort(retainedIds);
    return retainedIds;
}

}  // namespace skygate::ephemeris::tests
