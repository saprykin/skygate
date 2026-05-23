#pragma once

#include "SkyContext.hpp"
#include "BodyTrailOptions.hpp"
#include "BodyTrailSample.hpp"
#include "IEphemerisEngine.hpp"

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
