#pragma once

#include "BodyTrailOptions.hpp"
#include "BodyTrailSample.hpp"
#include "ObservationContext.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <cstdint>
#include <vector>

namespace skygate::ephemeris {

class BodyTrailCalculator final {
public:
    [[nodiscard]] std::vector<BodyTrailSample> sample(
        const IEphemerisEngine& engine,
        const core::ObservationContext& context,
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
