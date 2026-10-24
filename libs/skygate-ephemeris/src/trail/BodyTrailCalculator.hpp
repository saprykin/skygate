#pragma once

#include "BodyTrailOptions.hpp"
#include "BodyTrailSample.hpp"
#include "HorizontalCoordinate.hpp"
#include "ObservationContext.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace skygate::ephemeris {

class BodyTrailCalculator final {
public:
    // The render grid of one trail: one sample per step across the whole
    // past/future window, each holding its offset only. Callers that fill the
    // samples from interpolated anchors share this grid.
    [[nodiscard]] static std::vector<BodyTrailSample> sampleOffsets(const BodyTrailOptions& options);

    // Moves the request to the given offset in minutes and asks the engine for
    // the body state there. Nullopt when the engine has no finite state.
    [[nodiscard]] static std::optional<skygate::core::HorizontalCoordinate> sampleAtOffset(
        const IEphemerisEngine& engine, const EphemerisRequest& request, std::uint32_t bodyIndex, int offsetMinutes
    );

    [[nodiscard]] std::vector<BodyTrailSample> sample(
        const IEphemerisEngine& engine,
        const skygate::core::ObservationContext& context,
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
