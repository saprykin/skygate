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

    // Request-based overloads treat request.epoch as the authoritative
    // computation instant. Engines must use the explicit epoch rather than
    // request.context.utcTime. An engine that cannot honor an explicit non-UTC
    // epoch must report an unsupported-input result instead of falling back to
    // context.utcTime.
    [[nodiscard]] virtual EphemerisSnapshot compute(const EphemerisRequest& request) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const = 0;

    // Observation-context overloads are a convenience API for callers that
    // only have an observer and a UTC instant. They construct an
    // EphemerisRequest from the engine's current options and a UTC epoch
    // derived from context.utcTime (EphemerisRequestFactory::requestFromContext),
    // so they intentionally use defaults for corrections, precision, and
    // fallback. The derived UTC epoch is the authoritative instant. Callers
    // that need explicit epoch or correction control should build an
    // EphemerisRequest instead.
    [[nodiscard]] virtual EphemerisSnapshot compute(const skygate::core::ObservationContext& context) const = 0;
    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::string_view bodyId) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::uint32_t bodyIndex) const = 0;
};

}  // namespace skygate::ephemeris
