#pragma once

#include "engine/EphemerisEngineKind.hpp"

namespace skygate::ephemeris {

struct EphemerisEngineFactoryResult;

class EphemerisEngineReplacementPolicy final {
public:
    EphemerisEngineReplacementPolicy() = delete;

    /// Returns true when the engine factory degraded to a fallback while the
    /// controller already owns an engine of the requested kind. A degraded
    /// fallback creation never replaces an already-active engine of the
    /// requested kind.
    ///
    /// Retention is decided by the requested kind, the current engine's
    /// effective kind, and the creation outcome. Callers remain responsible
    /// for rebuilding the engine when the catalog or data changes.
    [[nodiscard]] static bool shouldKeepCurrentEngine(
        EphemerisEngineKind::Type requestedKind,
        const EphemerisEngineFactoryResult& creationResult,
        EphemerisEngineKind::Type currentEngineKind
    ) noexcept;
};

}  // namespace skygate::ephemeris
