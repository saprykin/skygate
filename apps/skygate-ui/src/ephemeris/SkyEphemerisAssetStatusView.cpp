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
    switch (m_source) {
    case SkyEphemerisActiveSource::Installed:
        return installedDatasetInfoText(m_snapshot);
    case SkyEphemerisActiveSource::MissingInstalledFallback:
        return QStringLiteral("Bundled fallback (installed data missing)");
    case SkyEphemerisActiveSource::BundledFallback:
        break;
    }
    return QStringLiteral("Bundled fallback");
}

QString SkyEphemerisAssetStatusView::shortRangeKernelStatusText() const
{
    return assetStatus(m_snapshot.installedKernelVersion).statusText();
}

QString SkyEphemerisAssetStatusView::longRangeKernelStatusText() const
{
    if (usingInstalledData() && m_snapshot.installedKernelLongRange.value_or(false)) {
        const QString version = m_snapshot.installedKernelVersion.isEmpty() ? QStringLiteral("installed")
                                                                            : m_snapshot.installedKernelVersion;
        return SkyEphemerisAssetStatus::installed(version).statusText();
    }
    return QStringLiteral("Not installed");
}

QString SkyEphemerisAssetStatusView::kernelDisplayText() const
{
    if (!usingInstalledData()) {
        return QStringLiteral("Bundled");
    }

    const std::optional<bool> installedKernelLongRange = m_snapshot.installedKernelLongRange;
    if (!installedKernelLongRange.has_value()) {
        return QStringLiteral("Bundled");
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

QString SkyEphemerisAssetStatusView::dataRevisionToken() const
{
    return m_snapshot.dataRevisionToken;
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
