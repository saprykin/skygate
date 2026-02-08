#pragma once

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(
    const IStarCatalog* catalog
);
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngineStub(
    const IStarCatalog* catalog
);

}  // namespace skygate::ephemeris
