#pragma once

#include "engine/EphemerisEngineKind.hpp"

namespace skygate::ephemeris {

struct EphemerisEngineFactoryResult;

class EphemerisEngineReplacementPolicy final {
public:
    EphemerisEngineReplacementPolicy() = delete;

    /// Returns true when a failed high-precision rebuild produced a simple
    /// fallback engine while the controller already owns a high-precision
    /// engine. In that case the existing high-precision engine should be kept.
    [[nodiscard]] static bool shouldKeepCurrentEngine(
        EphemerisEngineKind::Type requestedKind,
        const EphemerisEngineFactoryResult& creationResult,
        EphemerisEngineKind::Type currentEngineKind
    ) noexcept;
};

}  // namespace skygate::ephemeris
