#include "EphemerisEngineFactory.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "IEphemerisDiagnosticsSink.hpp"
#include "StringUtilities.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/DeltaTDataLoader.hpp"
#include "engine/highprecision/EarthOrientationDataLoader.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "engine/highprecision/IEarthOrientationProvider.hpp"
#include "engine/highprecision/IEphemerisDataSnapshot.hpp"
#include "engine/highprecision/ITimeScaleService.hpp"
#include "engine/highprecision/LeapSecondTableLoader.hpp"
#include "engine/highprecision/LeapSecondTimeScaleService.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"
#include "engine/highprecision/TimeScaleServiceOptions.hpp"
#include "engine/simple/SimpleEphemerisEngine.hpp"
#include "engine/simple/SimpleEphemerisFallbackStrategy.hpp"

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
    const skygate::ephemeris::highprecision::ICalcephKernel& kernel
)
{
    const EphemerisFactoryCreationDiagnosticCode code =
        kernel.status() == skygate::ephemeris::highprecision::ICalcephKernel::Status::CalcephUnavailable
            ? EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable
            : EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable;

    if (kernel.diagnostics().empty()) {
        diagnostics.push_back(makeDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error));
        return;
    }

    for (const std::string& diagnosticText : kernel.diagnostics()) {
        diagnostics.push_back(makeDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error, diagnosticText));
    }
}

void appendKernelDateRangeIfMissing(
    EphemerisDatasetInfo& dataSetInfo, const skygate::ephemeris::highprecision::ICalcephKernel& kernel
)
{
    const std::optional<skygate::ephemeris::highprecision::ICalcephKernel::Info>& kernelInfo = kernel.kernelInfo();
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
prefersPlanetarySystemBarycenters(const skygate::ephemeris::highprecision::ICalcephKernel& kernel) noexcept
{
    const std::optional<skygate::ephemeris::highprecision::ICalcephKernel::Info>& kernelInfo = kernel.kernelInfo();
    if (!kernelInfo.has_value()) {
        return false;
    }

    return StringUtilities::containsIgnoreAsciiCase(kernelInfo->id, "de440")
           || StringUtilities::containsIgnoreAsciiCase(kernelInfo->version, "de440")
           || StringUtilities::containsIgnoreAsciiCase(kernelInfo->id, "de441")
           || StringUtilities::containsIgnoreAsciiCase(kernelInfo->version, "de441");
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

[[nodiscard]] std::shared_ptr<const IEarthOrientationProvider>
loadEarthOrientationProviderFromSnapshot(const IEphemerisDataSnapshot& snapshot)
{
    const EarthOrientationDataLoader::Result result = EarthOrientationDataLoader::loadFromSnapshot(snapshot);
    return result.isSuccess() ? result.provider : nullptr;
}

[[nodiscard]] std::shared_ptr<const ITimeScaleService> loadTimeScaleServiceFromSnapshot(
    const IEphemerisDataSnapshot& snapshot,
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider
)
{
    const LeapSecondTableLoader::Result leapSecondTable = LeapSecondTableLoader::loadFromSnapshot(snapshot);
    if (!leapSecondTable.isSuccess()) {
        return nullptr;
    }

    const DeltaTDataLoader::Result deltaTData = DeltaTDataLoader::loadFromSnapshot(snapshot);
    TimeScaleServiceOptions timeScaleOptions;
    timeScaleOptions.allowDegradedLeapSecondFallback = true;
    timeScaleOptions.allowUt1DeltaTFallback = true;
    timeScaleOptions.earthOrientationSampleOptions.allowOutOfRangeNearestSampleFallback = true;
    timeScaleOptions.earthOrientationSampleOptions.allowMissingDataZeroFallback = true;
    timeScaleOptions.earthOrientationSampleOptions.degradePredictedData = false;

    return std::make_shared<LeapSecondTimeScaleService>(
        leapSecondTable.provider,
        timeScaleOptions,
        earthOrientationProvider,
        deltaTData.isSuccess() ? deltaTData.provider : nullptr
    );
}

void populateProvidersFromSnapshot(EphemerisEngineFactoryRequest& request)
{
    if (request.activeDataSnapshot == nullptr) {
        return;
    }
    if (request.timeScaleService != nullptr && request.earthOrientationProvider != nullptr) {
        return;
    }

    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider = request.earthOrientationProvider;
    if (earthOrientationProvider == nullptr) {
        earthOrientationProvider = loadEarthOrientationProviderFromSnapshot(*request.activeDataSnapshot);
    }
    if (request.timeScaleService == nullptr) {
        request.timeScaleService =
            loadTimeScaleServiceFromSnapshot(*request.activeDataSnapshot, earthOrientationProvider);
    }
    request.earthOrientationProvider = earthOrientationProvider;
}

[[nodiscard]] EphemerisEngineFactoryResult createHighPrecisionEngine(const EphemerisEngineFactoryRequest& request)
{
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;
    const bool needsDefaultCalcephKernelProvider = request.calcephKernelProvider == nullptr;

    if (needsDefaultCalcephKernelProvider && request.activeDataSnapshot == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An active ephemeris data snapshot is required for high-precision engine creation."
        ));
    }
    if (needsDefaultCalcephKernelProvider && request.dataManifest == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An ephemeris data manifest is required for high-precision engine creation."
        ));
    }
    if (request.datasetManifest == nullptr && request.dataManifest == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An ephemeris dataset manifest is required for high-precision engine creation."
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
        std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernelProvider> kernelProvider =
            request.calcephKernelProvider;
        if (kernelProvider == nullptr) {
            skygate::ephemeris::highprecision::CalcephKernelProvider::Options kernelSelectionOptions;
            kernelSelectionOptions.verifyChecksum = false;
            kernelProvider = std::make_shared<skygate::ephemeris::highprecision::CalcephKernelProvider>(
                *request.activeDataSnapshot, *request.dataManifest, std::move(kernelSelectionOptions)
            );
        }

        const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
            kernelProvider->openKernel();
        if (kernel == nullptr) {
            diagnostics.push_back(makeDiagnostic(
                EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed,
                EphemerisFactoryCreationDiagnosticSeverity::Error,
                "The CALCEPH kernel provider did not return a kernel."
            ));
        } else if (kernel->status() != skygate::ephemeris::highprecision::ICalcephKernel::Status::Ready) {
            appendCalcephDiagnostics(diagnostics, *kernel);
        } else {
            skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine::Dependencies dependencies;
            dependencies.calcephKernel = kernel;
            dependencies.solarSystemStateCalculator =
                std::make_shared<skygate::ephemeris::highprecision::SolarSystemStateCalculator>(
                    kernel, prefersPlanetarySystemBarycenters(*kernel)
                );
            dependencies.starAstrometryCalculator =
                std::make_shared<skygate::ephemeris::highprecision::StarAstrometryCalculator>(
                    kernel, request.timeScaleService
                );
            dependencies.timeScaleService = request.timeScaleService;
            dependencies.earthOrientationProvider = request.earthOrientationProvider;
            dependencies.frameTransformer = std::make_shared<skygate::ephemeris::highprecision::ErfaFrameTransformer>(
                request.timeScaleService, request.earthOrientationProvider
            );
            dependencies.atmosphericRefractionCalculator =
                std::make_shared<skygate::ephemeris::highprecision::AtmosphericRefractionCalculator>();
            dependencies.apparentPlaceCalculator =
                std::make_shared<skygate::ephemeris::highprecision::ApparentPlaceCalculator>(
                    dependencies.frameTransformer,
                    request.timeScaleService,
                    request.earthOrientationProvider,
                    dependencies.atmosphericRefractionCalculator
                );
            dependencies.computationCache =
                std::make_shared<skygate::ephemeris::highprecision::EphemerisComputationCache>();
            dependencies.fallbackStrategy = std::make_shared<SimpleEphemerisFallbackStrategy>();
            if (request.datasetManifest != nullptr) {
                dependencies.dataSetInfo = *request.datasetManifest;
            } else {
                dependencies.dataSetInfo = request.dataManifest->dataSetInfo;
            }
            appendKernelDateRangeIfMissing(dependencies.dataSetInfo, *kernel);

            return EphemerisEngineFactoryResult::success(
                std::make_unique<skygate::ephemeris::highprecision::HighPrecisionEphemerisEngine>(
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
    case EphemerisEngineKind::Type::Simple: {
        const EphemerisEngineFactoryRequest normalizedRequest = normalizeRequestOptions(request);
        result = EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(
                normalizedRequest.catalog != nullptr ? *normalizedRequest.catalog : CelestialBodyCatalog{},
                normalizedRequest.options
            )
        );
        break;
    }
    case EphemerisEngineKind::Type::HighPrecision: {
        EphemerisEngineFactoryRequest alignedRequest = normalizeRequestOptions(request);
        alignedRequest.fallbackPolicy = request.options.fallbackToSimpleEngine()
                                            ? EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback
                                            : EphemerisFactoryFallbackPolicy::StrictHighPrecision;
        populateProvidersFromSnapshot(alignedRequest);
        result = createHighPrecisionEngine(alignedRequest);
        break;
    }
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
    request.options = SimpleEphemerisEngine::defaultOptions();
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
    request.options = SimpleEphemerisEngine::defaultOptions();
    return create(request);
}

}  // namespace skygate::ephemeris
