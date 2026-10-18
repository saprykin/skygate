#include "EphemerisEngineFactory.hpp"
#include "EphemerisEngineFactoryRequest.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"
#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryDiagnostics.hpp"
#include "HighPrecisionEngineAssembly.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/simple/SimpleEphemerisEngine.hpp"

#include <initializer_list>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

std::string_view EphemerisEngineFactory::displayName(const EphemerisFactoryCreationStatus::Type status) noexcept
{
    return EphemerisFactoryCreationStatus::displayName(status);
}

bool EphemerisEngineFactory::isCreationSuccess(const EphemerisFactoryCreationStatus::Type status) noexcept
{
    return EphemerisFactoryCreationStatus::isSuccess(status);
}

namespace {

[[nodiscard]] EphemerisEngineFactoryResult
makeInvalidFactoryRequestResult(const EphemerisEngineKind::Type requestedKind)
{
    return EphemerisEngineFactoryResult::failure(
        EphemerisFactoryCreationStatus::Type::FailedInvalidRequest,
        {EphemerisFactoryCreationDiagnostic{
            EphemerisFactoryCreationDiagnostic::Code::InvalidRequest,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "The requested ephemeris engine kind is not supported.",
        }},
        requestedKind
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
            {},
            normalizedRequest.engineKind
        );
        break;
    }
    case EphemerisEngineKind::Type::HighPrecision:
        result = HighPrecisionEngineAssembly::assemble(normalizeRequestOptions(request));
        break;
    default:
        result = makeInvalidFactoryRequestResult(request.engineKind);
        break;
    }

    EphemerisFactoryDiagnostics::publish(request.diagnosticsSink, result.diagnostics);
    EphemerisFactoryDiagnostics::log(result.diagnostics);
    return result;
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create()
{
    EphemerisEngineFactoryRequest request;
    request.options = SimpleEphemerisEngine::defaultOptions();
    return create(request);
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const IStarCatalog& catalog)
{
    EphemerisEngineFactoryRequest request;
    request.catalog = std::make_shared<CelestialBodyCatalog>(catalog.bodies());
    request.options = SimpleEphemerisEngine::defaultOptions();
    return create(request);
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(std::initializer_list<OwnGalaxyCelestialBody> bodies)
{
    return create(std::span<const OwnGalaxyCelestialBody>{bodies.begin(), bodies.size()});
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(std::span<const OwnGalaxyCelestialBody> bodies)
{
    return create(CelestialBodyCatalog(std::vector<OwnGalaxyCelestialBody>{bodies.begin(), bodies.end()}));
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const CelestialBodyCatalog& catalog)
{
    EphemerisEngineFactoryRequest request;
    request.catalog = std::make_shared<CelestialBodyCatalog>(catalog);
    request.options = SimpleEphemerisEngine::defaultOptions();
    return create(request);
}

}  // namespace skygate::ephemeris
