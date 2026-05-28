#pragma once

#include "CelestialBodyState.hpp"
#include "EphemerisCapabilities.hpp"
#include "EphemerisDatasetInfo.hpp"
#include "EphemerisEngineKind.hpp"
#include "EphemerisEngineOptions.hpp"
#include "EphemerisRequest.hpp"
#include "EphemerisSnapshot.hpp"
#include "ObservationContext.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class IEphemerisEngine {
public:
    virtual ~IEphemerisEngine() = default;

    [[nodiscard]] virtual EphemerisEngineKind::Type kind() const noexcept
    {
        return EphemerisEngineKind::Type::Simple;
    }

    [[nodiscard]] virtual std::string_view name() const noexcept
    {
        return "Ephemeris engine";
    }

    [[nodiscard]] virtual EphemerisCapabilities capabilities() const noexcept
    {
        return EphemerisCapabilities::noCapabilities();
    }

    [[nodiscard]] virtual std::span<const EphemerisDateRange> supportedDateRanges() const noexcept
    {
        return {};
    }

    [[nodiscard]] virtual EphemerisDatasetInfo dataSetInfo() const
    {
        return {};
    }

    [[nodiscard]] virtual EphemerisEngineOptions options() const noexcept
    {
        EphemerisEngineOptions engineOptions;
        engineOptions.setEngineKind(kind());
        return engineOptions;
    }

    [[nodiscard]] virtual EphemerisSnapshot compute(const EphemerisRequest& request) const
    {
        return compute(request.context);
    }

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const
    {
        return computeBodyState(request.context, bodyId);
    }

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const
    {
        if (bodyIndex > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }

        return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
    }

    [[nodiscard]] virtual EphemerisSnapshot compute(const core::ObservationContext& context) const = 0;
    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const core::ObservationContext& context, std::string_view bodyId) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const core::ObservationContext& context, std::uint32_t bodyIndex) const = 0;
};

}  // namespace skygate::ephemeris
