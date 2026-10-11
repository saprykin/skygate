#pragma once

#include "EphemerisRequest.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "factory/EphemerisFactoryCreationDiagnostic.hpp"
#include "time/EphemerisDateRange.hpp"

#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace skygate::ephemeris {
class EphemerisBackendResourceCache;
class EphemerisDataManifest;
class EphemerisDatasetInfo;
class IEarthOrientationProvider;
class IEphemerisDataSnapshot;
class IEphemerisDiagnosticsSink;
class IEphemerisEngine;
class IEphemerisGuidanceStrategy;
class IStarCatalog;
class ITimeScaleService;
namespace highprecision {
class ICalcephKernelProvider;
}
}  // namespace skygate::ephemeris

// Owns the selected ephemeris engine for the context facade: engine creation
// and replacement, the backend resource cache, the default guidance strategy,
// the option revision, and request assembly. Catalog, data, and revision
// inputs arrive as explicit parameters; QML settings presentation stays in
// the facade.
//
// The engine is held by `shared_ptr` and replaced under a mutex. Handles
// returned by `engineHandle()` keep their engine alive, so a retained handle
// remains usable after a later replacement.
class SkyEphemerisSession final {
public:
    struct FactoryInputs final {
        const skygate::ephemeris::EphemerisDatasetInfo* datasetManifest = nullptr;
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
        std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernelProvider> calcephKernelProvider;
        skygate::ephemeris::IEphemerisDiagnosticsSink* diagnosticsSink = nullptr;
    };

    struct RebuildInputs final {
        const skygate::ephemeris::IStarCatalog* starCatalog = nullptr;
        std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot;
        const skygate::ephemeris::EphemerisDataManifest* dataManifest = nullptr;
        std::uint64_t ephemerisDataRevision = 0U;
    };

    struct RequestInputs final {
        skygate::core::ObservationContext observationContext;
        std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot;
        const skygate::ephemeris::EphemerisDataManifest* dataManifest = nullptr;
        std::uint64_t ephemerisDataRevision = 0U;
        std::uint64_t catalogRevision = 0U;
        QString installedEarthOrientationVersion;
        QString installedLeapSecondTableVersion;
        QString installedDeltaTDataVersion;
    };

    struct RequestContext final {
        skygate::ephemeris::EphemerisRequest request;
        std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot;
        std::uint64_t engineOptionsRevision = 0U;
        std::uint64_t ephemerisDataRevision = 0U;
        std::uint64_t earthOrientationDataRevision = 0U;
        std::uint64_t leapSecondDataRevision = 0U;
        std::uint64_t catalogRevision = 0U;
        std::optional<skygate::ephemeris::EphemerisDateRange> earthOrientationDataRange;
        std::optional<skygate::ephemeris::EphemerisDateRange> leapSecondTableRange;
        std::optional<skygate::ephemeris::EphemerisDateRange> deltaTDataRange;
    };

    using CreationDiagnosticsHandler =
        std::function<void(const std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic>&)>;

    explicit SkyEphemerisSession(
        std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> initialEngine, FactoryInputs factoryInputs
    );
    ~SkyEphemerisSession();

    SkyEphemerisSession(const SkyEphemerisSession&) = delete;
    SkyEphemerisSession& operator=(const SkyEphemerisSession&) = delete;
    SkyEphemerisSession(SkyEphemerisSession&&) = delete;
    SkyEphemerisSession& operator=(SkyEphemerisSession&&) = delete;

    [[nodiscard]] const skygate::ephemeris::IEphemerisEngine* engine() const noexcept;
    [[nodiscard]] std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> engineHandle() const;
    [[nodiscard]] std::shared_ptr<skygate::ephemeris::IEphemerisGuidanceStrategy> guidanceStrategy() const noexcept;

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type engineKind() const noexcept;
    [[nodiscard]] const skygate::ephemeris::EphemerisEngineOptions& engineOptions() const noexcept;

    /// Returns true when the kind or options changed and the option revision
    /// advanced. The caller owns the rebuild that follows.
    [[nodiscard]] bool applyOptions(
        skygate::ephemeris::EphemerisEngineKind::Type engineKind,
        const skygate::ephemeris::EphemerisEngineOptions& engineOptions
    );

    /// Creates the engine for the requested kind and installs it unless the
    /// replacement policy keeps the current engine. Creation diagnostics are
    /// delivered before any replacement so the facade can publish status text
    /// in the existing order.
    void rebuild(const RebuildInputs& inputs, const CreationDiagnosticsHandler& diagnosticsHandler);

    [[nodiscard]] RequestContext requestContext(const RequestInputs& inputs) const;

private:
    std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> m_engine;
    std::shared_ptr<skygate::ephemeris::IEphemerisGuidanceStrategy> m_guidanceStrategy;
    mutable std::mutex m_engineMutex;
    skygate::ephemeris::EphemerisEngineKind::Type m_engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    skygate::ephemeris::EphemerisEngineOptions m_engineOptions;
    std::uint64_t m_engineOptionsRevision = 1U;
    const skygate::ephemeris::EphemerisDatasetInfo* m_datasetManifest = nullptr;
    std::unique_ptr<skygate::ephemeris::EphemerisBackendResourceCache> m_backendResourceCache;
    skygate::ephemeris::IEphemerisDiagnosticsSink* m_diagnosticsSink = nullptr;
};
