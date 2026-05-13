#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class IEphemerisEngine {
public:
    virtual ~IEphemerisEngine() = default;

    [[nodiscard]] virtual EphemerisEngineKind kind() const noexcept
    {
        return EphemerisEngineKind::Simple;
    }

    [[nodiscard]] virtual std::string_view name() const noexcept
    {
        return "Ephemeris engine";
    }

    [[nodiscard]] virtual EphemerisCapabilities capabilities() const noexcept
    {
        EphemerisCapabilities engineCapabilities;
        engineCapabilities.engineKind = kind();
        return engineCapabilities;
    }

    [[nodiscard]] virtual std::span<const EphemerisDateRange> supportedDateRanges() const noexcept
    {
        return {};
    }

    [[nodiscard]] virtual EphemerisDataSetInfo dataSetInfo() const
    {
        return {};
    }

    [[nodiscard]] virtual EphemerisEngineOptions options() const noexcept
    {
        EphemerisEngineOptions engineOptions;
        engineOptions.engineKind = kind();
        return engineOptions;
    }

    [[nodiscard]] virtual SkySnapshot compute(const core::SkyContext& context) const = 0;
    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, std::string_view bodyId) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, std::uint32_t bodyIndex) const = 0;
};

}  // namespace skygate::ephemeris
