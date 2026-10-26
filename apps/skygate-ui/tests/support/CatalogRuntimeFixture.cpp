#include "CatalogRuntimeFixture.hpp"
#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/SkyCatalogRuntimeBuildOptions.hpp"
#include "catalog/SkyCatalogRuntimeResult.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::OwnGalaxyCelestialBody
makeHipCrossIdentifiedBody(std::string id, std::string hip, std::string displayName, const double magnitude = 1.0)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body =
        skygate::ui::tests::CatalogRuntimeFixture::makeFixedBody(std::move(id), std::move(displayName), magnitude);
    body.identity.externalIdentifiers.push_back(skygate::ephemeris::CatalogIdentifier::make("hip", std::move(hip)));
    return body;
}

}  // namespace

namespace skygate::ui::tests {

skygate::ephemeris::OwnGalaxyCelestialBody
CatalogRuntimeFixture::makeFixedBody(std::string id, std::string displayName, const double magnitude)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.visualMagnitude = magnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogRuntimeFixture::makeCatalog()
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeFixedBody("hip_1", "HIP 1"), makeFixedBody("hip_2", "HIP 2", 2.0)}
    );
}

std::unique_ptr<skygate::ephemeris::IStarCatalog>
CatalogRuntimeFixture::makeSingleStarCatalog(std::string id, std::string displayName)
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeFixedBody(std::move(id), std::move(displayName))}
    );
}

std::unique_ptr<skygate::ephemeris::IStarCatalog>
CatalogRuntimeFixture::makeHipCrossIdentifiedCatalog(std::string firstId, std::string secondId)
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeHipCrossIdentifiedBody(std::move(firstId), "1", "Alpha"),
         makeHipCrossIdentifiedBody(std::move(secondId), "2", "Beta", 2.0)}
    );
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogRuntimeFixture::makeCrossIdentifiedCatalog()
{
    return makeHipCrossIdentifiedCatalog("catalog_a_1", "catalog_a_2");
}

skygate::ephemeris::DistantCelestialBody
CatalogRuntimeFixture::makeMessierBody(std::string id, std::string messierNumber, const double magnitude)
{
    skygate::ephemeris::DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = "M" + messierNumber;
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = magnitude;
    body.identity.externalIdentifiers.push_back(
        skygate::ephemeris::CatalogIdentifier::make("messier", std::move(messierNumber))
    );
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    body.deepSkyObject =
        skygate::ephemeris::DeepSkyObjectInfo{.kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy};
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogRuntimeFixture::makeMixedCatalog(
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> ownGalaxyBodies,
    std::vector<skygate::ephemeris::DistantCelestialBody> distantBodies
)
{
    return skygate::ephemeris::tests::CatalogMergeTestSupport::createCatalog(
        std::move(ownGalaxyBodies), std::move(distantBodies)
    );
}

skygate::ui::internal::SkyCatalogRuntimeResult CatalogRuntimeFixture::applySingleStarSource(
    skygate::ui::internal::SkyCatalogRuntime& runtime,
    const QString& instanceId,
    std::string bodyId,
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions& options
)
{
    return runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = instanceId,
            .title = instanceId,
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog(std::move(bodyId), "Star"),
            .foundObjectCount = 0,
        },
        options
    );
}

bool CatalogRuntimeFixture::runtimeContainsBody(
    const skygate::ui::internal::SkyCatalogRuntime& runtime, const std::string& id
)
{
    const skygate::ephemeris::IStarCatalog* catalog = runtime.starCatalog();
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&id](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && body->id == id;
    });
}

skygate::ui::internal::SkyCatalogRuntimeBuildOptions CatalogRuntimeFixture::bundledDeepSkyOptions(
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation participation
)
{
    return skygate::ui::internal::SkyCatalogRuntimeBuildOptions{.bundledDeepSkyParticipation = participation};
}

}  // namespace skygate::ui::tests
