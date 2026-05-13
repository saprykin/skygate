#pragma once

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

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

[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine();
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(const IStarCatalog& catalog);
[[nodiscard]] std::unique_ptr<IEphemerisEngine> createEphemerisEngine(std::span<const CelestialBody> bodies);

}  // namespace skygate::ephemeris
