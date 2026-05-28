#include "EphemerisEngineFactory.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "IEphemerisDiagnosticsSink.hpp"
#include "StringUtilities.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/FrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"
#include "engine/simple/SimpleEphemerisEngine.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

bool EphemerisEngineFactory::allowsSimpleEngineFallback(const EphemerisFactoryFallbackPolicy policy) noexcept
{
    return policy == EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
}

std::string_view EphemerisEngineFactory::displayName(const EphemerisFactoryFallbackPolicy policy) noexcept
{
    switch (policy) {
    case EphemerisFactoryFallbackPolicy::StrictHighPrecision:
        return "strict high precision";
    case EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback:
        return "allow simple engine fallback";
    }

    return {};
}

std::string_view EphemerisEngineFactory::displayName(const EphemerisFactoryCreationStatus status) noexcept
{
    switch (status) {
    case EphemerisFactoryCreationStatus::CreatedRequestedEngine:
        return "created requested engine";
    case EphemerisFactoryCreationStatus::CreatedSimpleFallback:
        return "created simple engine fallback";
    case EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable:
        return "strict high precision unavailable";
    case EphemerisFactoryCreationStatus::FailedInvalidRequest:
        return "invalid factory request";
    case EphemerisFactoryCreationStatus::FailedCreationError:
        return "engine creation failed";
    }

    return {};
}

bool EphemerisEngineFactory::isCreationSuccess(const EphemerisFactoryCreationStatus status) noexcept
{
    return status == EphemerisFactoryCreationStatus::CreatedRequestedEngine
           || status == EphemerisFactoryCreationStatus::CreatedSimpleFallback;
}

std::string_view EphemerisEngineFactory::diagnosticText(const EphemerisFactoryCreationDiagnosticCode code) noexcept
{
    switch (code) {
    case EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable:
        return "High-precision ephemeris creation is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable:
        return "Required ephemeris data is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::RequiredTimeScaleServiceUnavailable:
        return "Required time-scale service is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::RequiredEarthOrientationProviderUnavailable:
        return "Required Earth-orientation provider is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::InvalidRequest:
        return "The ephemeris engine factory request is invalid.";
    case EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed:
        return "Ephemeris engine creation failed.";
    }

    return "Ephemeris engine creation diagnostic.";
}

namespace {

[[nodiscard]] EphemerisFactoryCreationDiagnostic makeDiagnostic(
    const EphemerisFactoryCreationDiagnosticCode code,
    const EphemerisFactoryCreationDiagnosticSeverity severity,
    std::string text = {}
)
{
    return {code, severity, std::move(text)};
}

[[nodiscard]] EphemerisEngineFactoryResult makeInvalidFactoryRequestResult()
{
    return EphemerisEngineFactoryResult::failure(
        EphemerisFactoryCreationStatus::FailedInvalidRequest,
        {EphemerisFactoryCreationDiagnostic{
            EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "The requested ephemeris engine kind is not supported.",
        }}
    );
}

[[nodiscard]] EphemerisFactoryCreationStatus
highPrecisionFailureStatus(const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics) noexcept
{
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed
            || diagnostic.code == EphemerisFactoryCreationDiagnosticCode::InvalidRequest) {
            return EphemerisFactoryCreationStatus::FailedCreationError;
        }
    }

    return EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable;
}

[[nodiscard]] std::vector<EphemerisFactoryCreationDiagnostic> withSeverity(
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics,
    const EphemerisFactoryCreationDiagnosticSeverity severity
)
{
    for (EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        diagnostic.severity = severity;
    }

    return diagnostics;
}

void appendCalcephDiagnostics(
    std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics,
    const highprecision::CalcephKernelProvider& kernelProvider
)
{
    const EphemerisFactoryCreationDiagnosticCode code =
        kernelProvider.status() == highprecision::CalcephKernelProviderStatus::CalcephUnavailable
            ? EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable
            : EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable;

    if (kernelProvider.diagnostics().empty()) {
        diagnostics.push_back(makeDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error));
        return;
    }

    for (const std::string& diagnosticText : kernelProvider.diagnostics()) {
        diagnostics.push_back(makeDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error, diagnosticText));
    }
}

void appendKernelDateRangeIfMissing(
    EphemerisDatasetInfo& dataSetInfo, const highprecision::CalcephKernelProvider& kernelProvider
)
{
    const std::optional<highprecision::CalcephKernelInfo>& kernelInfo = kernelProvider.kernelInfo();
    if (!kernelInfo.has_value()) {
        return;
    }

    for (const EphemerisDateRange& range : dataSetInfo.dateRanges) {
        if (range.id == kernelInfo->validityRange.id) {
            return;
        }
    }

    dataSetInfo.dateRanges.push_back(kernelInfo->validityRange);
}

[[nodiscard]] bool
prefersPlanetarySystemBarycenters(const highprecision::CalcephKernelProvider& kernelProvider) noexcept
{
    const std::optional<highprecision::CalcephKernelInfo>& kernelInfo = kernelProvider.kernelInfo();
    if (!kernelInfo.has_value()) {
        return false;
    }

    return StringUtilities::containsIgnoreAsciiCase(kernelInfo->id, "de440")
           || StringUtilities::containsIgnoreAsciiCase(kernelInfo->version, "de440")
           || StringUtilities::containsIgnoreAsciiCase(kernelInfo->id, "de441")
           || StringUtilities::containsIgnoreAsciiCase(kernelInfo->version, "de441");
}

[[nodiscard]] EphemerisEngineFactoryResult createHighPrecisionEngine(const EphemerisEngineFactoryRequest& request)
{
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;

    if (request.activeDataSnapshot == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An active ephemeris data snapshot is required for high-precision engine creation."
        ));
    }
    if (request.dataManifest == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An ephemeris data manifest is required for high-precision engine creation."
        ));
    }
    if (request.timeScaleService == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredTimeScaleServiceUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "A time-scale service is required for high-precision engine creation."
        ));
    }
    if (request.earthOrientationProvider == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEarthOrientationProviderUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An Earth-orientation provider is required for high-precision engine creation."
        ));
    }

    if (diagnostics.empty()) {
        highprecision::CalcephKernelSelectionOptions kernelSelectionOptions;
        kernelSelectionOptions.verifyChecksum = false;
        auto kernelProvider = std::make_shared<highprecision::CalcephKernelProvider>(
            *request.activeDataSnapshot,
            *request.dataManifest,
            std::move(kernelSelectionOptions),
            request.calcephKernelRuntime
        );
        if (!kernelProvider->isReady()) {
            appendCalcephDiagnostics(diagnostics, *kernelProvider);
        } else {
            highprecision::HighPrecisionEphemerisEngineDependencies dependencies;
            dependencies.calcephKernelProvider = kernelProvider;
            dependencies.solarSystemStateCalculator = std::make_shared<highprecision::SolarSystemStateCalculator>(
                kernelProvider, prefersPlanetarySystemBarycenters(*kernelProvider)
            );
            dependencies.starAstrometryCalculator =
                std::make_shared<highprecision::StarAstrometryCalculator>(kernelProvider, request.timeScaleService);
            dependencies.timeScaleService = request.timeScaleService;
            dependencies.earthOrientationProvider = request.earthOrientationProvider;
            dependencies.frameTransformer = std::make_shared<highprecision::ErfaFrameTransformer>(
                request.timeScaleService, request.earthOrientationProvider
            );
            dependencies.atmosphericRefractionCalculator =
                std::make_shared<highprecision::AtmosphericRefractionCalculator>();
            dependencies.apparentPlaceCalculator = std::make_shared<highprecision::ApparentPlaceCalculator>(
                dependencies.frameTransformer,
                request.timeScaleService,
                request.earthOrientationProvider,
                dependencies.atmosphericRefractionCalculator
            );
            dependencies.computationCache = std::make_shared<highprecision::EphemerisComputationCache>();
            dependencies.dataSetInfo = request.dataManifest->dataSetInfo;
            if (request.dataSetManifest != nullptr) {
                dependencies.dataSetInfo = *request.dataSetManifest;
            }
            appendKernelDateRangeIfMissing(dependencies.dataSetInfo, *kernelProvider);

            return EphemerisEngineFactoryResult::success(
                std::make_unique<highprecision::HighPrecisionEphemerisEngine>(
                    request.catalog != nullptr ? *request.catalog : CelestialBodyCatalog{},
                    request.options,
                    std::move(dependencies)
                )
            );
        }
    }

    if (EphemerisEngineFactory::allowsSimpleEngineFallback(request.fallbackPolicy)) {
        return EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(
                request.catalog != nullptr ? *request.catalog : CelestialBodyCatalog{}, request.options
            ),
            EphemerisFactoryCreationStatus::CreatedSimpleFallback,
            withSeverity(std::move(diagnostics), EphemerisFactoryCreationDiagnosticSeverity::Warning)
        );
    }

    return EphemerisEngineFactoryResult::failure(highPrecisionFailureStatus(diagnostics), std::move(diagnostics));
}

void publishDiagnostics(
    IEphemerisDiagnosticsSink* diagnosticsSink, const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics
)
{
    if (diagnosticsSink == nullptr) {
        return;
    }

    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        diagnosticsSink->recordFactoryCreationDiagnostic(diagnostic);
    }
}

}  // namespace

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const EphemerisEngineFactoryRequest& request)
{
    EphemerisEngineFactoryResult result;
    switch (request.engineKind) {
    case EphemerisEngineKind::Type::Simple:
        result = EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(
                request.catalog != nullptr ? *request.catalog : CelestialBodyCatalog{}, request.options
            )
        );
        break;
    case EphemerisEngineKind::Type::HighPrecision:
        result = createHighPrecisionEngine(request);
        break;
    default:
        result = makeInvalidFactoryRequestResult();
        break;
    }

    publishDiagnostics(request.diagnosticsSink, result.diagnostics);
    return result;
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create()
{
    EphemerisEngineFactoryRequest request;
    request.options = simpleEphemerisEngineDefaultOptions();
    return create(request);
}

EphemerisEngineFactoryResult EphemerisEngineFactory::create(const IStarCatalog& catalog)
{
    return create(catalog.catalog());
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
    request.options = simpleEphemerisEngineDefaultOptions();
    return create(request);
}

}  // namespace skygate::ephemeris
