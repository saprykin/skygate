#include "EphemerisBackendResourceCache.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/TimeScaleProviderLoader.hpp"
#include "factory/EphemerisEngineFactoryRequest.hpp"
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include "engine/highprecision/CalcephKernelProvider.hpp"
#endif

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

    if (m_earthOrientationProvider == nullptr) {
        m_earthOrientationProvider = TimeScaleProviderLoader::loadEarthOrientationProvider(*activeDataSnapshot);
        m_cacheManaged = true;
    }
    if (m_timeScaleService == nullptr) {
        m_timeScaleService =
            TimeScaleProviderLoader::loadTimeScaleService(*activeDataSnapshot, m_earthOrientationProvider);
        m_cacheManaged = true;
    }
    if (m_calcephKernelProvider == nullptr && dataManifest != nullptr) {
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        // Installed ephemeris data is checksum-verified when it is activated.
        // Re-hashing the (potentially multi-gigabyte) kernel on every engine
        // rebuild would dominate startup time, so the cached kernel provider
        // skips per-open checksum verification.
        highprecision::CalcephKernelProvider::Options kernelOptions;
        kernelOptions.verifyChecksum = false;
        m_calcephKernelProvider = std::make_shared<highprecision::CalcephKernelProvider>(
            *activeDataSnapshot, *dataManifest, std::move(kernelOptions)
        );
        m_cacheManaged = true;
#endif
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
