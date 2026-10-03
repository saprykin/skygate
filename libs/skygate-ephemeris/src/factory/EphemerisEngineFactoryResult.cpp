#include "EphemerisEngineFactoryResult.hpp"

#include <utility>

namespace skygate::ephemeris {

EphemerisEngineFactoryResult EphemerisEngineFactoryResult::success(
    std::unique_ptr<IEphemerisEngine> createdEngine,
    const EphemerisFactoryCreationStatus creationStatus,
    std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics,
    const EphemerisEngineKind::Type requestedKind
)
{
    EphemerisEngineFactoryResult result;
    if (createdEngine != nullptr) {
        result.m_effectiveKind = createdEngine->kind();
    }
    result.engine = std::move(createdEngine);
    result.status = creationStatus;
    result.diagnostics = std::move(creationDiagnostics);
    result.m_requestedKind = requestedKind;
    return result;
}

EphemerisEngineFactoryResult EphemerisEngineFactoryResult::failure(
    const EphemerisFactoryCreationStatus creationStatus,
    std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics,
    const EphemerisEngineKind::Type requestedKind
)
{
    EphemerisEngineFactoryResult result;
    result.status = creationStatus;
    result.diagnostics = std::move(creationDiagnostics);
    result.m_requestedKind = requestedKind;
    return result;
}

bool EphemerisEngineFactoryResult::isSuccess() const noexcept
{
    return engine != nullptr
           && (status == EphemerisFactoryCreationStatus::CreatedRequestedEngine
               || status == EphemerisFactoryCreationStatus::CreatedSimpleFallback);
}

bool EphemerisEngineFactoryResult::isFailure() const noexcept
{
    return !isSuccess();
}

bool EphemerisEngineFactoryResult::usedSimpleEngineFallback() const noexcept
{
    return status == EphemerisFactoryCreationStatus::CreatedSimpleFallback;
}

EphemerisEngineKind::Type EphemerisEngineFactoryResult::requestedKind() const noexcept
{
    return m_requestedKind;
}

EphemerisEngineKind::Type EphemerisEngineFactoryResult::effectiveKind() const noexcept
{
    if (m_effectiveKind.has_value()) {
        return *m_effectiveKind;
    }

    return m_requestedKind;
}

bool EphemerisEngineFactoryResult::hasDiagnostics() const noexcept
{
    return !diagnostics.empty();
}

bool EphemerisEngineFactoryResult::hasErrors() const noexcept
{
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.isError()) {
            return true;
        }
    }

    return false;
}

}  // namespace skygate::ephemeris
