#include "EphemerisBackendResourceCache.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/TimeScaleProviderLoader.hpp"
#include "factory/EphemerisEngineFactoryRequest.hpp"

#include <utility>

namespace skygate::ephemeris {

EphemerisBackendResourceCache::EphemerisBackendResourceCache(InitialProviders initialProviders)
    : m_timeScaleService(std::move(initialProviders.timeScaleService)),
      m_earthOrientationProvider(std::move(initialProviders.earthOrientationProvider)),
      m_calcephKernelProvider(std::move(initialProviders.calcephKernelProvider))
{
}

EphemerisBackendResourceCache::~EphemerisBackendResourceCache() = default;

void EphemerisBackendResourceCache::refresh(
    const std::shared_ptr<const IEphemerisDataSnapshot>& activeDataSnapshot,
    const EphemerisDataManifest* dataManifest,
    const std::uint64_t dataRevision
)
{
    if (m_cacheManaged && (m_cacheRevision != dataRevision || m_cacheManifest != dataManifest)) {
        m_timeScaleService.reset();
        m_earthOrientationProvider.reset();
        m_calcephKernelProvider.reset();
        m_cacheManaged = false;
    }
    m_cacheRevision = dataRevision;
    m_cacheManifest = dataManifest;

    if (activeDataSnapshot == nullptr) {
        return;
    }

    // Installed ephemeris data is checksum-verified when it is activated.
    // Re-hashing the (potentially multi-gigabyte) kernel on every engine
    // rebuild would dominate startup time, so the cached kernel provider
    // skips per-open checksum verification.
    const TimeScaleProviderLoader::Providers resolved = TimeScaleProviderLoader::resolveMissing(
        *activeDataSnapshot,
        dataManifest,
        TimeScaleProviderLoader::Providers{
            .timeScaleService = m_timeScaleService,
            .earthOrientationProvider = m_earthOrientationProvider,
            .calcephKernelProvider = m_calcephKernelProvider,
        },
        TimeScaleProviderLoader::KernelOptions{.verifyChecksum = false}
    );

    if (resolved.timeScaleService != m_timeScaleService) {
        m_timeScaleService = resolved.timeScaleService;
        m_cacheManaged = true;
    }
    if (resolved.earthOrientationProvider != m_earthOrientationProvider) {
        m_earthOrientationProvider = resolved.earthOrientationProvider;
        m_cacheManaged = true;
    }
    if (resolved.calcephKernelProvider != m_calcephKernelProvider) {
        m_calcephKernelProvider = resolved.calcephKernelProvider;
        m_cacheManaged = true;
    }
}

void EphemerisBackendResourceCache::applyProviderFields(EphemerisEngineFactoryRequest& request) const noexcept
{
    request.timeScaleService = m_timeScaleService;
    request.earthOrientationProvider = m_earthOrientationProvider;
    request.calcephKernelProvider = m_calcephKernelProvider;
}

std::shared_ptr<const ITimeScaleService> EphemerisBackendResourceCache::timeScaleService() const noexcept
{
    return m_timeScaleService;
}

std::shared_ptr<const IEarthOrientationProvider>
EphemerisBackendResourceCache::earthOrientationProvider() const noexcept
{
    return m_earthOrientationProvider;
}

std::shared_ptr<const highprecision::ICalcephKernelProvider>
EphemerisBackendResourceCache::calcephKernelProvider() const noexcept
{
    return m_calcephKernelProvider;
}

}  // namespace skygate::ephemeris
