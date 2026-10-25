#include "EphemerisEngineFactoryResult.hpp"

#include <utility>

namespace skygate::ephemeris {

EphemerisEngineFactoryResult EphemerisEngineFactoryResult::success(
    std::unique_ptr<IEphemerisEngine> createdEngine,
    const EphemerisFactoryCreationStatus::Type creationStatus,
    std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics
)
{
    EphemerisEngineFactoryResult result;
    result.engine = std::move(createdEngine);
    result.status = creationStatus;
    result.diagnostics = std::move(creationDiagnostics);
    return result;
}

EphemerisEngineFactoryResult EphemerisEngineFactoryResult::failure(
    const EphemerisFactoryCreationStatus::Type creationStatus,
    std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics
)
{
    EphemerisEngineFactoryResult result;
    result.status = creationStatus;
    result.diagnostics = std::move(creationDiagnostics);
    return result;
}

bool EphemerisEngineFactoryResult::usedSimpleEngineFallback() const noexcept
{
    return status == EphemerisFactoryCreationStatus::Type::CreatedSimpleFallback;
}

}  // namespace skygate::ephemeris
