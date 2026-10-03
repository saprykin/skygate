#pragma once

#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisEngineFactoryResult {
    std::unique_ptr<IEphemerisEngine> engine;
    EphemerisFactoryCreationStatus status = EphemerisFactoryCreationStatus::FailedCreationError;
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;

    [[nodiscard]] static EphemerisEngineFactoryResult success(
        std::unique_ptr<IEphemerisEngine> createdEngine,
        EphemerisFactoryCreationStatus creationStatus = EphemerisFactoryCreationStatus::CreatedRequestedEngine,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics = {},
        EphemerisEngineKind::Type requestedKind = EphemerisEngineKind::Type::Simple
    );

    [[nodiscard]] static EphemerisEngineFactoryResult failure(
        EphemerisFactoryCreationStatus creationStatus,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics,
        EphemerisEngineKind::Type requestedKind = EphemerisEngineKind::Type::Simple
    );

    [[nodiscard]] bool isSuccess() const noexcept;
    [[nodiscard]] bool isFailure() const noexcept;
    [[nodiscard]] bool usedSimpleEngineFallback() const noexcept;
    [[nodiscard]] EphemerisEngineKind::Type requestedKind() const noexcept;
    // The kind of the engine the factory actually produced. For success
    // results this is captured from the created engine at construction time,
    // so it remains stable after engine ownership moves out of the result.
    // For failure results it falls back to requestedKind().
    [[nodiscard]] EphemerisEngineKind::Type effectiveKind() const noexcept;
    [[nodiscard]] bool hasDiagnostics() const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;

private:
    EphemerisEngineKind::Type m_requestedKind = EphemerisEngineKind::Type::Simple;
    std::optional<EphemerisEngineKind::Type> m_effectiveKind;
};

}  // namespace skygate::ephemeris
