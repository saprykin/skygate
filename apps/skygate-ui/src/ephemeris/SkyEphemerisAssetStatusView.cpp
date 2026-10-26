#include "SkyEphemerisAssetStatusView.hpp"
#include "SkyEphemerisStatusText.hpp"

#include <QFileInfo>
#include <QStringList>

#include <optional>
#include <utility>

namespace skygate::ui::internal {
namespace {

QString installedDatasetInfoText(const SkyEphemerisDataCacheSnapshot& snapshot)
{
    QStringList parts;
    if (!snapshot.installedKernelVersion.isEmpty()) {
        parts.push_back(QStringLiteral("Kernel %1").arg(snapshot.installedKernelVersion));
    }
    if (!snapshot.installedEarthOrientationVersion.isEmpty()) {
        parts.push_back(QStringLiteral("EOP %1").arg(snapshot.installedEarthOrientationVersion));
    }
    if (!snapshot.installedLeapSecondTableVersion.isEmpty()) {
        parts.push_back(QStringLiteral("Leap seconds %1").arg(snapshot.installedLeapSecondTableVersion));
    }
    if (!snapshot.installedDeltaTDataVersion.isEmpty()) {
        parts.push_back(QStringLiteral("Delta T %1").arg(snapshot.installedDeltaTDataVersion));
    }
    if (parts.isEmpty()) {
        parts.push_back(QStringLiteral("Installed metadata"));
    }
    return parts.join(QStringLiteral(" | "));
}

}  // namespace

SkyEphemerisAssetStatusView::SkyEphemerisAssetStatusView(
    SkyEphemerisDataCacheSnapshot snapshot, const SkyEphemerisActiveSource source
)
    : m_snapshot(std::move(snapshot)), m_source(source)
{
}

QString SkyEphemerisAssetStatusView::statusText() const
{
    switch (m_source) {
    case SkyEphemerisActiveSource::Installed:
        return SkyEphemerisStatusText::installedDataActive();
    case SkyEphemerisActiveSource::MissingInstalledFallback:
        return SkyEphemerisStatusText::installedDataMissing();
    case SkyEphemerisActiveSource::BundledFallback:
        break;
    }
    return SkyEphemerisStatusText::bundledFallback();
}

QString SkyEphemerisAssetStatusView::datasetInfoText() const
{
    if (m_source == SkyEphemerisActiveSource::Installed) {
        return installedDatasetInfoText(m_snapshot);
    }

    // The fallback labels come from the status type, so the dataset info and the
    // status text of the same state cannot drift apart.
    return assetStatus(QString()).statusText();
}

QString SkyEphemerisAssetStatusView::shortRangeKernelStatusText() const
{
    return assetStatus(m_snapshot.installedKernelVersion).statusText();
}

QString SkyEphemerisAssetStatusView::kernelDisplayText() const
{
    const std::optional<bool> installedKernelLongRange = m_snapshot.installedKernelLongRange;
    if (!usingInstalledData() || !installedKernelLongRange.has_value()) {
        return SkyEphemerisAssetStatus::bundledFallback().displayText();
    }
    return *installedKernelLongRange ? QStringLiteral("DE441") : QStringLiteral("DE440s");
}

QString SkyEphemerisAssetStatusView::earthOrientationStatusText() const
{
    return assetStatus(m_snapshot.installedEarthOrientationVersion).statusText();
}

QString SkyEphemerisAssetStatusView::leapSecondStatusText() const
{
    return assetStatus(m_snapshot.installedLeapSecondTableVersion).statusText();
}

QString SkyEphemerisAssetStatusView::deltaTStatusText() const
{
    return assetStatus(m_snapshot.installedDeltaTDataVersion).statusText();
}

std::uint64_t SkyEphemerisAssetStatusView::planetaryKernelCacheSizeBytes() const
{
    const QFileInfo kernelFileInfo(m_snapshot.installedKernelPath);
    if (!kernelFileInfo.exists() || !kernelFileInfo.isFile() || kernelFileInfo.size() <= 0) {
        return 0U;
    }
    return static_cast<std::uint64_t>(kernelFileInfo.size());
}

std::uint64_t SkyEphemerisAssetStatusView::supportDataCacheSizeBytes() const
{
    std::uint64_t totalBytes = 0U;
    for (const QString& path : SkyEphemerisDataCacheSnapshot::installedSupportDataPaths(m_snapshot)) {
        const QFileInfo fileInfo(path);
        if (fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0) {
            totalBytes += static_cast<std::uint64_t>(fileInfo.size());
        }
    }
    return totalBytes;
}

QString SkyEphemerisAssetStatusView::lastUpdateResultText() const
{
    return m_snapshot.lastUpdateResult;
}

bool SkyEphemerisAssetStatusView::usingInstalledData() const noexcept
{
    return m_source == SkyEphemerisActiveSource::Installed;
}

SkyEphemerisAssetStatus SkyEphemerisAssetStatusView::assetStatus(const QString& installedVersion) const
{
    if (usingInstalledData() && !installedVersion.isEmpty()) {
        return SkyEphemerisAssetStatus::installed(installedVersion);
    }
    if (m_source == SkyEphemerisActiveSource::MissingInstalledFallback) {
        return SkyEphemerisAssetStatus::missingInstalledFallback();
    }
    return SkyEphemerisAssetStatus::bundledFallback();
}

}  // namespace skygate::ui::internal
