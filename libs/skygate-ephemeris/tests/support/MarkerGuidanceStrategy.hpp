#pragma once

#include "MarkerGuidanceEngine.hpp"

#include "engine/IEphemerisGuidanceStrategy.hpp"

#include <memory>

namespace skygate::ephemeris::tests {

class MarkerGuidanceStrategy final : public IEphemerisGuidanceStrategy {
public:
    [[nodiscard]] std::unique_ptr<IEphemerisEngine>
    createGuidanceEngine(const CelestialBodyCatalog& catalog) const override;

    mutable int createCount = 0;
    std::shared_ptr<int> m_sampleCount = std::make_shared<int>(0);
};

}  // namespace skygate::ephemeris::tests
