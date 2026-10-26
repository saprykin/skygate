#pragma once

#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <memory>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisEngineFactoryResult {
    std::unique_ptr<IEphemerisEngine> engine;
    EphemerisFactoryCreationStatus::Type status = EphemerisFactoryCreationStatus::Type::FailedCreationError;
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;

    [[nodiscard]] static EphemerisEngineFactoryResult success(
        std::unique_ptr<IEphemerisEngine> createdEngine,
        EphemerisFactoryCreationStatus::Type creationStatus =
            EphemerisFactoryCreationStatus::Type::CreatedRequestedEngine,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics = {}
    );

    [[nodiscard]] static EphemerisEngineFactoryResult failure(
        EphemerisFactoryCreationStatus::Type creationStatus,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics
    );

    [[nodiscard]] bool usedSimpleEngineFallback() const noexcept;
};

}  // namespace skygate::ephemeris
