#include "HighPrecisionEngineAssembly.hpp"
#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"
#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryDiagnostics.hpp"
#include "EphemerisFactoryFallbackPolicy.hpp"
#include "StringUtilities.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleProviderLoader.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "engine/highprecision/ICalcephKernelProvider.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"
#include "engine/simple/SimpleEphemerisEngine.hpp"
#include "engine/simple/SimpleEphemerisFallbackStrategy.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

void appendCalcephDiagnostics(
    std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics,
    const skygate::ephemeris::highprecision::ICalcephKernel& kernel
)
{
    const EphemerisFactoryCreationDiagnostic::Code code =
        kernel.status() == skygate::ephemeris::highprecision::ICalcephKernel::Status::CalcephUnavailable
            ? EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable
            : EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable;

    if (kernel.diagnostics().empty()) {
        diagnostics.push_back(
            EphemerisFactoryDiagnostics::creationDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error)
        );
        return;
    }

    for (const std::string& diagnosticText : kernel.diagnostics()) {
        diagnostics.push_back(
            EphemerisFactoryDiagnostics::creationDiagnostic(
                code, EphemerisFactoryCreationDiagnosticSeverity::Error, diagnosticText
            )
        );
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

void populateProvidersFromSnapshot(EphemerisEngineFactoryRequest& request)
{
    if (request.activeDataSnapshot == nullptr) {
        return;
    }

    // A factory request is a one-shot creation, so kernel providers composed
    // here verify their checksum. The backend resource cache, which reuses one
    // provider across engine rebuilds, opts out of the re-read explicitly.
    const TimeScaleProviderLoader::Providers resolved = TimeScaleProviderLoader::resolveMissing(
        *request.activeDataSnapshot,
        request.dataManifest,
        TimeScaleProviderLoader::Providers{
            .timeScaleService = request.timeScaleService,
            .earthOrientationProvider = request.earthOrientationProvider,
            .calcephKernelProvider = request.calcephKernelProvider,
        },
        TimeScaleProviderLoader::KernelOptions{.verifyChecksum = true}
    );
    request.timeScaleService = resolved.timeScaleService;
    request.earthOrientationProvider = resolved.earthOrientationProvider;
    request.calcephKernelProvider = resolved.calcephKernelProvider;
}

[[nodiscard]] EphemerisEngineFactoryResult createHighPrecisionEngine(const EphemerisEngineFactoryRequest& request)
{
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;
    const bool needsDefaultCalcephKernelProvider = request.calcephKernelProvider == nullptr;
    const auto require =
        [&diagnostics](
            const bool satisfied, const EphemerisFactoryCreationDiagnostic::Code code, std::string diagnosticText
        ) {
            if (satisfied) {
                return;
            }

            diagnostics.push_back(
                EphemerisFactoryDiagnostics::creationDiagnostic(
                    code, EphemerisFactoryCreationDiagnosticSeverity::Error, std::move(diagnosticText)
                )
            );
        };

    require(
        !needsDefaultCalcephKernelProvider || request.activeDataSnapshot != nullptr,
        EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable,
        "An active ephemeris data snapshot is required for high-precision engine creation."
    );
    require(
        !needsDefaultCalcephKernelProvider || request.dataManifest != nullptr,
        EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable,
        "An ephemeris data manifest is required for high-precision engine creation."
    );
    require(
        request.datasetManifest != nullptr || request.dataManifest != nullptr,
        EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable,
        "An ephemeris dataset manifest is required for high-precision engine creation."
    );
    require(
        request.timeScaleService != nullptr,
        EphemerisFactoryCreationDiagnostic::Code::RequiredTimeScaleServiceUnavailable,
        "A time-scale service is required for high-precision engine creation."
    );
    require(
        request.earthOrientationProvider != nullptr,
        EphemerisFactoryCreationDiagnostic::Code::RequiredEarthOrientationProviderUnavailable,
        "An Earth-orientation provider is required for high-precision engine creation."
    );

    if (diagnostics.empty()) {
        const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernelProvider> kernelProvider =
            request.calcephKernelProvider;
        const std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernel> kernel =
            kernelProvider != nullptr ? kernelProvider->openKernel() : nullptr;
        if (kernel == nullptr) {
            diagnostics.push_back(
                EphemerisFactoryDiagnostics::creationDiagnostic(
                    EphemerisFactoryCreationDiagnostic::Code::EngineCreationFailed,
                    EphemerisFactoryCreationDiagnosticSeverity::Error,
                    "The CALCEPH kernel provider did not return a kernel."
                )
            );
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
                ),
                EphemerisFactoryCreationStatus::Type::CreatedRequestedEngine,
                {}
            );
        }
    }

    if (EphemerisFactoryFallbackPolicy::allowsSimpleEngineFallback(request.fallbackPolicy)) {
        return EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(
                request.catalog != nullptr ? *request.catalog : CelestialBodyCatalog{}, request.options
            ),
            EphemerisFactoryCreationStatus::Type::CreatedSimpleFallback,
            EphemerisFactoryDiagnostics::withSeverity(
                std::move(diagnostics), EphemerisFactoryCreationDiagnosticSeverity::Warning
            )
        );
    }

    return EphemerisEngineFactoryResult::failure(
        EphemerisFactoryDiagnostics::failureStatus(diagnostics), std::move(diagnostics)
    );
}

}  // namespace

EphemerisEngineFactoryResult HighPrecisionEngineAssembly::assemble(EphemerisEngineFactoryRequest request)
{
    populateProvidersFromSnapshot(request);
    return createHighPrecisionEngine(request);
}

}  // namespace skygate::ephemeris
