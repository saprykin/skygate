#pragma once

#include "CelestialBodyCatalog.hpp"
#include "EphemerisFactoryFallbackPolicy.hpp"
#include "engine/EphemerisDatasetInfo.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"

#include <memory>

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
    EphemerisEngineKind::Type engineKind = EphemerisEngineKind::Type::Simple;
    std::shared_ptr<const CelestialBodyCatalog> catalog;
    EphemerisEngineOptions options;
    const EphemerisDatasetInfo* dataSetManifest = nullptr;
    const EphemerisDataManifest* dataManifest = nullptr;
    std::shared_ptr<const IEphemerisDataSnapshot> activeDataSnapshot;
    std::shared_ptr<const ITimeScaleService> timeScaleService;
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider;
    std::shared_ptr<const highprecision::ICalcephKernelRuntime> calcephKernelRuntime;
    EphemerisFactoryFallbackPolicy fallbackPolicy = EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    IEphemerisDiagnosticsSink* diagnosticsSink = nullptr;
};

}  // namespace skygate::ephemeris
