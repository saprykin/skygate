#pragma once

#include "EphemerisFactoryFallbackPolicy.hpp"
#include "IEphemerisEngine.hpp"

#include <memory>
#include <span>

namespace skygate::ephemeris {

class IEarthOrientationProvider;
struct EphemerisDataManifest;
class IEphemerisDataSnapshot;
class IEphemerisDiagnosticsSink;
class ITimeScaleService;

namespace highprecision {
class ICalcephKernelRuntime;
}  // namespace highprecision

struct EphemerisEngineFactoryRequest {
    EphemerisEngineKind engineKind = EphemerisEngineKind::Simple;
    std::span<const CelestialBody> catalogBodies;
    EphemerisEngineOptions options;
    const EphemerisDataSetInfo* dataSetManifest = nullptr;
    const EphemerisDataManifest* dataManifest = nullptr;
    std::shared_ptr<const IEphemerisDataSnapshot> activeDataSnapshot;
    std::shared_ptr<const ITimeScaleService> timeScaleService;
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider;
    std::shared_ptr<const highprecision::ICalcephKernelRuntime> calcephKernelRuntime;
    EphemerisFactoryFallbackPolicy fallbackPolicy = EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    IEphemerisDiagnosticsSink* diagnosticsSink = nullptr;
};

}  // namespace skygate::ephemeris
