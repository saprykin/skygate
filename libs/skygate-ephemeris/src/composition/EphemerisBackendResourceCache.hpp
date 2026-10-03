#pragma once

#include "engine/IEarthOrientationProvider.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/highprecision/ICalcephKernelProvider.hpp"

#include <cstdint>
#include <memory>

namespace skygate::ephemeris {

class EphemerisDataManifest;
class IEphemerisDataSnapshot;
struct EphemerisEngineFactoryRequest;

/// Caches and composes the backend resources shared by ephemeris engine
/// rebuilds. The cache owns time-scale, Earth-orientation, and CALCEPH kernel
/// providers, invalidates them when the active ephemeris data revision or
/// manifest changes, and copies them into factory requests.
class EphemerisBackendResourceCache final {
public:
    struct InitialProviders final {
        std::shared_ptr<const ITimeScaleService> timeScaleService;
        std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider;
        std::shared_ptr<const highprecision::ICalcephKernelProvider> calcephKernelProvider;
    };

    EphemerisBackendResourceCache() = default;
    explicit EphemerisBackendResourceCache(InitialProviders initialProviders);
    ~EphemerisBackendResourceCache();

    EphemerisBackendResourceCache(const EphemerisBackendResourceCache&) = delete;
    EphemerisBackendResourceCache& operator=(const EphemerisBackendResourceCache&) = delete;
    EphemerisBackendResourceCache(EphemerisBackendResourceCache&&) noexcept = delete;
    EphemerisBackendResourceCache& operator=(EphemerisBackendResourceCache&&) noexcept = delete;

    void refresh(
        const std::shared_ptr<const IEphemerisDataSnapshot>& activeDataSnapshot,
        const EphemerisDataManifest* dataManifest,
        std::uint64_t dataRevision
    );

    void applyProviderFields(EphemerisEngineFactoryRequest& request) const noexcept;

    [[nodiscard]] std::shared_ptr<const ITimeScaleService> timeScaleService() const noexcept;
    [[nodiscard]] std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider() const noexcept;
    [[nodiscard]] std::shared_ptr<const highprecision::ICalcephKernelProvider> calcephKernelProvider() const noexcept;

private:
    std::shared_ptr<const ITimeScaleService> m_timeScaleService;
    std::shared_ptr<const IEarthOrientationProvider> m_earthOrientationProvider;
    std::shared_ptr<const highprecision::ICalcephKernelProvider> m_calcephKernelProvider;
    bool m_cacheManaged = false;
    std::uint64_t m_cacheRevision = 0U;
    const EphemerisDataManifest* m_cacheManifest = nullptr;
};

}  // namespace skygate::ephemeris
