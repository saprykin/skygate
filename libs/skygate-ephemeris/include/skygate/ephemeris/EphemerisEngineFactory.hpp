#pragma once

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

class IEarthOrientationProvider;
class IEphemerisDataSnapshot;
class IEphemerisDiagnosticsSink;
class ITimeScaleService;

enum class EphemerisFactoryFallbackPolicy : std::uint8_t {
    StrictHighPrecision,
    AllowSimpleEngineFallback
};

[[nodiscard]] constexpr bool allowsSimpleEngineFallback(const EphemerisFactoryFallbackPolicy policy) noexcept
{
    return policy == EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
}

[[nodiscard]] constexpr std::string_view displayName(const EphemerisFactoryFallbackPolicy policy) noexcept
{
    switch (policy) {
    case EphemerisFactoryFallbackPolicy::StrictHighPrecision:
        return "strict high precision";
    case EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback:
        return "allow simple engine fallback";
    }

    return {};
}

struct EphemerisEngineFactoryRequest {
    EphemerisEngineKind engineKind = EphemerisEngineKind::Simple;
    std::span<const CelestialBody> catalogBodies;
    EphemerisEngineOptions options;
    const EphemerisDataSetInfo* dataSetManifest = nullptr;
    std::shared_ptr<const IEphemerisDataSnapshot> activeDataSnapshot;
    std::shared_ptr<const ITimeScaleService> timeScaleService;
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider;
    EphemerisFactoryFallbackPolicy fallbackPolicy = EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
    IEphemerisDiagnosticsSink* diagnosticsSink = nullptr;
};

enum class EphemerisFactoryCreationStatus : std::uint8_t {
    CreatedRequestedEngine,
    CreatedSimpleFallback,
    FailedStrictHighPrecisionUnavailable,
    FailedInvalidRequest,
    FailedCreationError
};

[[nodiscard]] constexpr std::string_view displayName(const EphemerisFactoryCreationStatus status) noexcept
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

[[nodiscard]] constexpr bool isFactoryCreationSuccess(const EphemerisFactoryCreationStatus status) noexcept
{
    return status == EphemerisFactoryCreationStatus::CreatedRequestedEngine
           || status == EphemerisFactoryCreationStatus::CreatedSimpleFallback;
}

enum class EphemerisFactoryCreationDiagnosticSeverity : std::uint8_t {
    Warning,
    Error
};

enum class EphemerisFactoryCreationDiagnosticCode : std::uint8_t {
    HighPrecisionUnavailable,
    RequiredEphemerisDataUnavailable,
    RequiredTimeScaleServiceUnavailable,
    RequiredEarthOrientationProviderUnavailable,
    InvalidRequest,
    EngineCreationFailed
};

[[nodiscard]] constexpr std::string_view
ephemerisFactoryCreationDiagnosticText(const EphemerisFactoryCreationDiagnosticCode code) noexcept
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

struct EphemerisFactoryCreationDiagnostic {
    EphemerisFactoryCreationDiagnosticCode code = EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed;
    EphemerisFactoryCreationDiagnosticSeverity severity = EphemerisFactoryCreationDiagnosticSeverity::Error;
    std::string diagnosticText;

    EphemerisFactoryCreationDiagnostic() = default;

    EphemerisFactoryCreationDiagnostic(
        const EphemerisFactoryCreationDiagnosticCode diagnosticCode,
        const EphemerisFactoryCreationDiagnosticSeverity diagnosticSeverity,
        std::string text = {}
    )
        : code(diagnosticCode), severity(diagnosticSeverity), diagnosticText(std::move(text))
    {
    }

    [[nodiscard]] std::string_view displayText() const noexcept
    {
        if (!diagnosticText.empty()) {
            return diagnosticText;
        }

        return ephemerisFactoryCreationDiagnosticText(code);
    }

    [[nodiscard]] bool isError() const noexcept
    {
        return severity == EphemerisFactoryCreationDiagnosticSeverity::Error;
    }
};

struct EphemerisEngineFactoryResult {
    std::unique_ptr<IEphemerisEngine> engine;
    EphemerisFactoryCreationStatus status = EphemerisFactoryCreationStatus::FailedCreationError;
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;

    [[nodiscard]] static EphemerisEngineFactoryResult success(
        std::unique_ptr<IEphemerisEngine> createdEngine,
        const EphemerisFactoryCreationStatus creationStatus = EphemerisFactoryCreationStatus::CreatedRequestedEngine,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics = {}
    )
    {
        EphemerisEngineFactoryResult result;
        result.engine = std::move(createdEngine);
        result.status = creationStatus;
        result.diagnostics = std::move(creationDiagnostics);
        return result;
    }

    [[nodiscard]] static EphemerisEngineFactoryResult failure(
        const EphemerisFactoryCreationStatus creationStatus,
        std::vector<EphemerisFactoryCreationDiagnostic> creationDiagnostics
    )
    {
        EphemerisEngineFactoryResult result;
        result.status = creationStatus;
        result.diagnostics = std::move(creationDiagnostics);
        return result;
    }

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return engine != nullptr && isFactoryCreationSuccess(status);
    }

    [[nodiscard]] bool isFailure() const noexcept
    {
        return !isSuccess();
    }

    [[nodiscard]] bool usedSimpleEngineFallback() const noexcept
    {
        return status == EphemerisFactoryCreationStatus::CreatedSimpleFallback;
    }

    [[nodiscard]] bool hasDiagnostics() const noexcept
    {
        return !diagnostics.empty();
    }

    [[nodiscard]] bool hasErrors() const noexcept
    {
        for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
            if (diagnostic.isError()) {
                return true;
            }
        }

        return false;
    }
};

[[nodiscard]] EphemerisEngineFactoryResult createEphemerisEngine(const EphemerisEngineFactoryRequest& request);
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine();
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(const IStarCatalog& catalog);
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(std::initializer_list<CelestialBody> bodies);
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(std::span<const CelestialBody> bodies);

}  // namespace skygate::ephemeris
