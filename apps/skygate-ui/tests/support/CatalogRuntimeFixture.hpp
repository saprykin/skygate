#pragma once

#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogRuntime.hpp"

#include <QString>

#include <memory>
#include <string>
#include <vector>

namespace skygate::ui::tests {

// Builds catalog bodies, composed catalogs, runtime sources, and provenance
// queries for the catalog runtime suites.
class CatalogRuntimeFixture final {
public:
    static constexpr skygate::ui::internal::SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation
        kDisabledBundledDeepSky =
            skygate::ui::internal::SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation::Disabled;
    static constexpr skygate::ui::internal::SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation
        kFallbackBundledDeepSky =
            skygate::ui::internal::SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation::Fallback;

    [[nodiscard]] static skygate::ephemeris::OwnGalaxyCelestialBody
    makeFixedBody(std::string id, std::string displayName, double magnitude = 1.0);

    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCatalog();

    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog>
    makeSingleStarCatalog(std::string id, std::string displayName);

    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog>
    makeHipCrossIdentifiedCatalog(std::string firstId, std::string secondId);

    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCrossIdentifiedCatalog();

    [[nodiscard]] static skygate::ephemeris::DistantCelestialBody
    makeMessierBody(std::string id, std::string messierNumber, double magnitude);

    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog> makeMixedCatalog(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> ownGalaxyBodies,
        std::vector<skygate::ephemeris::DistantCelestialBody> distantBodies
    );

    [[nodiscard]] static skygate::ui::internal::SkyCatalogRuntimeResult applySingleStarSource(
        skygate::ui::internal::SkyCatalogRuntime& runtime,
        const QString& instanceId,
        std::string bodyId,
        const skygate::ui::internal::SkyCatalogRuntimeBuildOptions& options
    );

    [[nodiscard]] static bool
    runtimeContainsBody(const skygate::ui::internal::SkyCatalogRuntime& runtime, const std::string& id);

    [[nodiscard]] static skygate::ui::internal::SkyCatalogRuntimeBuildOptions bundledDeepSkyOptions(
        skygate::ui::internal::SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation participation
    );
};

}  // namespace skygate::ui::tests
