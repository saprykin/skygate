#pragma once

#include "SkyEphemerisActiveSource.hpp"
#include "SkyEphemerisAssetStatus.hpp"
#include "SkyEphemerisDataCacheSnapshot.hpp"

#include <QString>

#include <cstdint>

namespace skygate::ui::internal {

// Display state of the active ephemeris data: the status line, the dataset
// and asset texts, the cache sizes, and the revision token. The view is built
// from one cache snapshot and the source it was restored from, so the data
// manager keeps the snapshot and its change signals while every displayed
// string comes from here.
class SkyEphemerisAssetStatusView final {
public:
    SkyEphemerisAssetStatusView() = default;
    SkyEphemerisAssetStatusView(SkyEphemerisDataCacheSnapshot snapshot, SkyEphemerisActiveSource source);

    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString datasetInfoText() const;
    [[nodiscard]] QString shortRangeKernelStatusText() const;
    [[nodiscard]] QString longRangeKernelStatusText() const;
    [[nodiscard]] QString kernelDisplayText() const;
    [[nodiscard]] QString earthOrientationStatusText() const;
    [[nodiscard]] QString leapSecondStatusText() const;
    [[nodiscard]] QString deltaTStatusText() const;
    [[nodiscard]] std::uint64_t planetaryKernelCacheSizeBytes() const;
    [[nodiscard]] std::uint64_t supportDataCacheSizeBytes() const;
    [[nodiscard]] QString lastUpdateResultText() const;
    [[nodiscard]] QString dataRevisionToken() const;

private:
    [[nodiscard]] bool usingInstalledData() const noexcept;
    [[nodiscard]] SkyEphemerisAssetStatus assetStatus(const QString& installedVersion) const;

    SkyEphemerisDataCacheSnapshot m_snapshot;
    SkyEphemerisActiveSource m_source = SkyEphemerisActiveSource::BundledFallback;
};

}  // namespace skygate::ui::internal
