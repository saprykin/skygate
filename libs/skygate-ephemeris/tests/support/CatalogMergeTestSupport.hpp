#pragma once

#include "BaseCelestialBody.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/IStarCatalog.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::tests {

// Builds catalog bodies and composes catalog collections for the identity,
// bridge-retention, source-precedence, row-origin, magnitude-precedence,
// and coordinate-model merge tests.
class CatalogMergeTestSupport final {
public:
    [[nodiscard]] static OwnGalaxyCelestialBody makeStar(
        std::string id,
        std::string displayName = {},
        std::vector<CatalogIdentifier> identifiers = {},
        std::vector<std::string> aliases = {},
        std::optional<double> rightAscensionHours = 1.0,
        std::optional<double> declinationDeg = 2.0,
        double visualMagnitude = std::numeric_limits<double>::quiet_NaN()
    );

    [[nodiscard]] static DistantCelestialBody makeDeepSkyObject(
        std::string id,
        std::string displayName = {},
        std::vector<std::string> aliases = {},
        std::vector<CatalogIdentifier> identifiers = {},
        std::optional<double> rightAscensionHours = 1.0,
        std::optional<double> declinationDeg = 2.0,
        std::optional<double> majorAxisArcmin = std::nullopt,
        double visualMagnitude = std::numeric_limits<double>::quiet_NaN()
    );

    [[nodiscard]] static std::unique_ptr<IStarCatalog>
    createCatalog(std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies);

    [[nodiscard]] static CatalogCompositionResult composePrimary(const IStarCatalog& source);

    [[nodiscard]] static CatalogCompositionResult
    composePrimaryWithDeepSky(const IStarCatalog& source, const IStarCatalog& deepSky);

    [[nodiscard]] static CatalogCompositionResult composeInOrder(const IStarCatalog& first, const IStarCatalog& second);

    [[nodiscard]] static CatalogCompositionResult composeAll(const std::vector<const IStarCatalog*>& catalogs);

    [[nodiscard]] static skygate::core::AstronomicalEpoch j2000Epoch();

    [[nodiscard]] static OwnGalaxyCelestialBody withReferenceModel(OwnGalaxyCelestialBody body);

    [[nodiscard]] static const BaseCelestialBody*
    findBodyById(std::span<const BaseCelestialBody* const> bodies, std::string_view id);

    [[nodiscard]] static std::size_t
    countBodiesById(std::span<const BaseCelestialBody* const> bodies, std::string_view id);

    [[nodiscard]] static bool
    hasIdentifier(const BaseCelestialBody& body, std::string_view namespaceName, std::string_view value);

    [[nodiscard]] static bool hasAlias(const std::vector<std::string>& aliases, std::string_view alias);

    [[nodiscard]] static bool hasRetainedCanonicalId(const BaseCelestialBody& body, std::string_view retainedId);

    [[nodiscard]] static std::vector<std::string> sortedIdentifierKeys(const BaseCelestialBody& body);

    [[nodiscard]] static std::vector<std::string> sortedRetainedCanonicalIds(const BaseCelestialBody& body);
};

}  // namespace skygate::ephemeris::tests
