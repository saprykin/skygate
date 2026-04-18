#pragma once

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <span>

namespace skygate::ephemeris {

[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine();
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(const IStarCatalog& catalog);
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(
    std::span<const CelestialBody> bodies
);

}  // namespace skygate::ephemeris
