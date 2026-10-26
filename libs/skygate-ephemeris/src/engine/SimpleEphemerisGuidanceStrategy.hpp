#pragma once

#include "IEphemerisGuidanceStrategy.hpp"

namespace skygate::ephemeris {

class SimpleEphemerisGuidanceStrategy final : public IEphemerisGuidanceStrategy {
public:
    [[nodiscard]] std::unique_ptr<IEphemerisEngine>
    createGuidanceEngine(const CelestialBodyCatalog& catalog) const override;
};

}  // namespace skygate::ephemeris
