#include "EphemerisEngineFactory.hpp"
#include "EphemerisEngineFactoryRequest.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"
#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryDiagnostics.hpp"
#include "HighPrecisionEngineAssembly.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/simple/SimpleEphemerisEngine.hpp"

#include <memory>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] EphemerisEngineFactoryResult makeInvalidFactoryRequestResult()
{
    return EphemerisEngineFactoryResult::failure(
        EphemerisFactoryCreationStatus::Type::FailedInvalidRequest,
        {EphemerisFactoryCreationDiagnostic{
            EphemerisFactoryCreationDiagnostic::Code::InvalidRequest,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "The requested ephemeris engine kind is not supported.",
        }}
    );
}

[[nodiscard]] EphemerisEngineFactoryRequest normalizeRequestOptions(EphemerisEngineFactoryRequest request)
{
    request.options.setEngineKind(request.engineKind);
    if (request.engineKind == EphemerisEngineKind::Type::Simple) {
        if (!request.options.isOptionSet(EphemerisEngineOptions::Key::CorrectionFlags)) {
            request.options.setCorrectionFlags(EphemerisCorrectionFlags::noCorrections());
        }
        if (!request.options.isOptionSet(EphemerisEngineOptions::Key::EnableAtmosphericRefraction)) {
            request.options.setEnableAtmosphericRefraction(false);
        }
    }

    return request;
}

}  // namespace

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const EphemerisEngineFactoryRequest& request)
{
    EphemerisEngineFactoryResult result;
    switch (request.engineKind) {
    case EphemerisEngineKind::Type::Simple: {
        const EphemerisEngineFactoryRequest normalizedRequest = normalizeRequestOptions(request);
        result = EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(
                normalizedRequest.catalog != nullptr ? *normalizedRequest.catalog : CelestialBodyCatalog{},
                normalizedRequest.options
            ),
            EphemerisFactoryCreationStatus::Type::CreatedRequestedEngine,
            {}
        );
        break;
    }
    case EphemerisEngineKind::Type::HighPrecision:
        result = HighPrecisionEngineAssembly::assemble(normalizeRequestOptions(request));
        break;
    default:
        result = makeInvalidFactoryRequestResult();
        break;
    }

    EphemerisFactoryDiagnostics::publish(request.diagnosticsSink, result.diagnostics);
    EphemerisFactoryDiagnostics::log(result.diagnostics);
    return result;
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const IStarCatalog& catalog)
{
    EphemerisEngineFactoryRequest request;
    request.catalog = std::make_shared<CelestialBodyCatalog>(catalog.bodies());
    request.options = SimpleEphemerisEngine::defaultOptions();
    return create(request);
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const CelestialBodyCatalog& catalog)
{
    EphemerisEngineFactoryRequest request;
    request.catalog = std::make_shared<CelestialBodyCatalog>(catalog);
    request.options = SimpleEphemerisEngine::defaultOptions();
    return create(request);
}

}  // namespace skygate::ephemeris
