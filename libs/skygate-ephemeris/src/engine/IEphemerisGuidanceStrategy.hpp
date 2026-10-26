#pragma once

#include "CelestialBodyCatalog.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <memory>

namespace skygate::ephemeris {

class IEphemerisGuidanceStrategy {
public:
    virtual ~IEphemerisGuidanceStrategy() = default;

    [[nodiscard]] virtual std::unique_ptr<IEphemerisEngine>
    createGuidanceEngine(const CelestialBodyCatalog& catalog) const = 0;
};

}  // namespace skygate::ephemeris
