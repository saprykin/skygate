#pragma once

#include "skygate/core/SkyContext.hpp"
#include "skygate/ephemeris/BodyTrailOptions.hpp"
#include "skygate/ephemeris/BodyTrailSample.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <cstdint>
#include <vector>

namespace skygate::ephemeris {

class BodyTrailCalculator final {
public:
    [[nodiscard]] std::vector<BodyTrailSample> sample(
        const IEphemerisEngine& engine,
        const core::SkyContext& context,
        std::uint32_t bodyIndex,
        const BodyTrailOptions& options = {}
    ) const;
    [[nodiscard]] std::vector<BodyTrailSample> sample(
        const IEphemerisEngine& engine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const BodyTrailOptions& options = {}
    ) const;
};

}  // namespace skygate::ephemeris
