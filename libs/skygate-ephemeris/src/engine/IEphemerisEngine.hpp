#pragma once

#include "CelestialBodyState.hpp"
#include "EphemerisCapabilities.hpp"
#include "EphemerisDatasetInfo.hpp"
#include "EphemerisEngineKind.hpp"
#include "EphemerisEngineOptions.hpp"
#include "EphemerisEngineTraits.hpp"
#include "EphemerisRequest.hpp"
#include "EphemerisSnapshot.hpp"
#include "ObservationContext.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

// Contract implemented by every ephemeris computation engine.
//
// kind() and name() are pure virtual so a concrete engine cannot silently
// inherit the Simple defaults; every production engine must declare an
// explicit identity. capabilities(), supportedDateRanges(), and dataSetInfo()
// retain default returns for lightweight test doubles, but production engines
// MUST override them with their real feature, range, and data-set metadata.
//
// Body indices are uniformly std::size_t across the request-based and
// observation-context overload families. Snapshot state indices remain
// std::uint32_t; call sites that forward a snapshot body index into an engine
// overload must widen it explicitly.
class IEphemerisEngine {
public:
    virtual ~IEphemerisEngine() = default;

    [[nodiscard]] virtual EphemerisEngineKind::Type kind() const noexcept = 0;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] virtual EphemerisCapabilities capabilities() const noexcept
    {
        return EphemerisCapabilities::noCapabilities();
    }

    // Traits advertise the policy choices an engine recommends to shared
    // computation consumers. They are advisory: consumers resolve behavior
    // through EphemerisComputationPolicy instead of branching on engine
    // identity directly.
    [[nodiscard]] virtual EphemerisEngineTraits traits() const noexcept
    {
        return EphemerisEngineTraits::noTraits();
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

    // Canonical request API.
    //
    // request.epoch is the authoritative computation instant when it is
    // explicit. Engines must use the explicit epoch rather than
    // request.context.utcTime. An engine that cannot honor an explicit epoch
    // (for example a non-UTC scale it does not support) must report an
    // unsupported-input result instead of silently falling back to
    // context.utcTime.
    [[nodiscard]] virtual EphemerisSnapshot compute(const EphemerisRequest& request) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const = 0;

    // Convenience API.
    //
    // Observation-context overloads adapt a UTC observer instant into an
    // EphemerisRequest using the engine's current options
    // (EphemerisRequestFactory::requestFromContext), so they intentionally use
    // engine defaults for corrections, precision, and fallback. The derived UTC
    // epoch is the authoritative instant. Callers that need explicit epoch or
    // correction control must build an EphemerisRequest instead.
    [[nodiscard]] virtual EphemerisSnapshot compute(const skygate::core::ObservationContext& context) const = 0;
    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::string_view bodyId) const = 0;

    [[nodiscard]] virtual std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::size_t bodyIndex) const = 0;
};

}  // namespace skygate::ephemeris
