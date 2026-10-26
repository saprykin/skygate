#include "SkyEphemerisStatusText.hpp"

#include <QString>

#include <string_view>

namespace skygate::ui::internal {
namespace {

// Wraps the status body in the class prefix, so the texts produced here and the
// presentations that strip the prefix read the same constant.
[[nodiscard]] QString withStatusPrefix(const QString& body)
{
    constexpr std::string_view prefix = SkyEphemerisStatusText::kStatusPrefix;
    return QString::fromLatin1(prefix.data(), static_cast<qsizetype>(prefix.size())) + body;
}

}  // namespace

QString SkyEphemerisStatusText::bundledFallback()
{
    return withStatusPrefix(QStringLiteral("Bundled fallback"));
}

QString SkyEphemerisStatusText::installedDataMissing()
{
    return withStatusPrefix(QStringLiteral("Installed data missing; bundled fallback active"));
}

QString SkyEphemerisStatusText::installedDataActive()
{
    return withStatusPrefix(QStringLiteral("Installed data active"));
}

QString SkyEphemerisStatusText::updateAlreadyInProgress()
{
    return withStatusPrefix(QStringLiteral("Update already in progress"));
}

QString SkyEphemerisStatusText::updateManifestUnavailable()
{
    return withStatusPrefix(QStringLiteral("Update manifest unavailable"));
}

QString SkyEphemerisStatusText::updateManifestUnreadable()
{
    return withStatusPrefix(QStringLiteral("Unable to open update manifest"));
}

QString SkyEphemerisStatusText::updateManifestTooLarge()
{
    return withStatusPrefix(QStringLiteral("Update manifest is too large"));
}

QString SkyEphemerisStatusText::updateManagerUnavailable()
{
    return withStatusPrefix(QStringLiteral("Update manager unavailable"));
}

QString SkyEphemerisStatusText::updateProfileUnavailable()
{
    return withStatusPrefix(QStringLiteral("Update profile unavailable"));
}

QString SkyEphemerisStatusText::updateProfileMissingAsset()
{
    return withStatusPrefix(QStringLiteral("Update profile references a missing asset"));
}

QString SkyEphemerisStatusText::writableCacheUnavailable()
{
    return withStatusPrefix(QStringLiteral("Writable cache unavailable"));
}

QString SkyEphemerisStatusText::stagingAreaUnclearable()
{
    return withStatusPrefix(QStringLiteral("Unable to clear update staging area"));
}

QString SkyEphemerisStatusText::stagingAreaUncreatable()
{
    return withStatusPrefix(QStringLiteral("Unable to create update staging area"));
}

QString SkyEphemerisStatusText::checkingUpdateManifest()
{
    return withStatusPrefix(QStringLiteral("Checking update manifest..."));
}

QString SkyEphemerisStatusText::downloading(const QString& progressName)
{
    return withStatusPrefix(QStringLiteral("Downloading %1...").arg(progressName));
}

QString SkyEphemerisStatusText::verifying(const QString& progressName)
{
    return withStatusPrefix(QStringLiteral("Verifying %1...").arg(progressName));
}

QString SkyEphemerisStatusText::cancelingDownload()
{
    return withStatusPrefix(QStringLiteral("Canceling download..."));
}

QString SkyEphemerisStatusText::checkingSupportData()
{
    return withStatusPrefix(QStringLiteral("Checking time and Earth data..."));
}

QString SkyEphemerisStatusText::supportDataUpdatesAvailable()
{
    return withStatusPrefix(QStringLiteral("Updates available for time and Earth data"));
}

QString SkyEphemerisStatusText::supportDataUpToDate()
{
    return withStatusPrefix(QStringLiteral("Time and Earth data is up to date"));
}

QString SkyEphemerisStatusText::failure(const QString& diagnostic)
{
    return withStatusPrefix(diagnostic);
}

}  // namespace skygate::ui::internal
