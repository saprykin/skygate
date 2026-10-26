#pragma once

#include "EphemerisEngineFactoryRequest.hpp"
#include "EphemerisEngineFactoryResult.hpp"
#include "catalog/IStarCatalog.hpp"

namespace skygate::ephemeris {

class EphemerisEngineFactory final {
public:
    [[nodiscard]] static EphemerisEngineFactoryResult create(const EphemerisEngineFactoryRequest& request);
    [[nodiscard]] static EphemerisEngineFactoryResult create(const IStarCatalog& catalog);
    [[nodiscard]] static EphemerisEngineFactoryResult create(const CelestialBodyCatalog& catalog);

    EphemerisEngineFactory() = delete;
};

}  // namespace skygate::ephemeris
