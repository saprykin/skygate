#pragma once

#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyContextController.hpp"
#include "engine/EphemerisDatasetInfo.hpp"

#include <string>
#include <vector>

namespace skygate::ui::tests {

// Fixture data shared by the performance guard suites: controller
// initialization options and the synthetic bodies, catalogs, and dataset
// metadata the guard scenarios measure.
class PerformanceGuardScenario final {
public:
    [[nodiscard]] static SkyContextController::InitializationOptions initializationOptions();

    [[nodiscard]] static skygate::ephemeris::OwnGalaxyCelestialBody makeBody(
        std::string id,
        std::string displayName,
        skygate::ephemeris::BaseCelestialBody::Kind type,
        double visualMagnitude,
        double rightAscensionHours,
        double declinationDeg
    );

    [[nodiscard]] static skygate::ephemeris::CelestialBodyCatalog
    makeLargeMixedCatalog(bool includeExactTarget = false);

    [[nodiscard]] static std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> makeHighPrecisionGuardCatalog();

    [[nodiscard]] static std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> makeHighPrecisionMoonGuardCatalog();

    [[nodiscard]] static skygate::ephemeris::EphemerisDatasetInfo makeHighPrecisionGuardDataSetInfo();
};

}  // namespace skygate::ui::tests
