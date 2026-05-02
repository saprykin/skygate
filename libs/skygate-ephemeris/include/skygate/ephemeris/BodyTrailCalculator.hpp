#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace skygate::ephemeris {

struct BodyTrailOptions final {
    int pastHours = 6;
    int futureHours = 18;
    int sampleStepMinutes = 30;
};

struct BodyTrailSample final {
    int offsetMinutes = 0;
    std::optional<core::HorizontalCoordinate> horizontal;
};

class BodyTrailCalculator final {
public:
    [[nodiscard]] std::vector<BodyTrailSample> sample(
        const IEphemerisEngine& engine,
        const core::SkyContext& context,
        std::uint32_t bodyIndex,
        const BodyTrailOptions& options = {}
    ) const;
};

}  // namespace skygate::ephemeris
