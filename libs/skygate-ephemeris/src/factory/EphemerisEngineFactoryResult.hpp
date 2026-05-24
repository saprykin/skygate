#pragma once

#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <memory>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisEngineFactoryResult {
    std::unique_ptr<IEphemerisEngine> engine;
    EphemerisFactoryCreationStatus status = EphemerisFactoryCreationStatus::FailedCreationError;
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;

    [[nodiscard]] static EphemerisEngineFactoryResult success(
        std::unique_ptr<IEphemerisEngine> createdEngine,
        EphemerisFactoryCreationStatus creationStatus = EphemerisFactoryCreationStatus::CreatedRequestedEngine,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics = {}
    );

    [[nodiscard]] static EphemerisEngineFactoryResult failure(
        EphemerisFactoryCreationStatus creationStatus,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics
    );

    [[nodiscard]] bool isSuccess() const noexcept;
    [[nodiscard]] bool isFailure() const noexcept;
    [[nodiscard]] bool usedSimpleEngineFallback() const noexcept;
    [[nodiscard]] bool hasDiagnostics() const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;
};

}  // namespace skygate::ephemeris
