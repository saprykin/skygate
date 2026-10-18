#include "SkyEphemerisStatusText.hpp"

namespace skygate::ui::internal {

QString SkyEphemerisStatusText::bundledFallback()
{
    return "Ephemeris data: Bundled fallback";
}

QString SkyEphemerisStatusText::installedDataMissing()
{
    return "Ephemeris data: Installed data missing; bundled fallback active";
}

QString SkyEphemerisStatusText::installedDataActive()
{
    return "Ephemeris data: Installed data active";
}

QString SkyEphemerisStatusText::updateAlreadyInProgress()
{
    return "Ephemeris data: Update already in progress";
}

QString SkyEphemerisStatusText::updateManifestUnavailable()
{
    return "Ephemeris data: Update manifest unavailable";
}

QString SkyEphemerisStatusText::updateManifestUnreadable()
{
    return "Ephemeris data: Unable to open update manifest";
}

QString SkyEphemerisStatusText::updateManifestTooLarge()
{
    return "Ephemeris data: Update manifest is too large";
}

QString SkyEphemerisStatusText::updateManagerUnavailable()
{
    return "Ephemeris data: Update manager unavailable";
}

QString SkyEphemerisStatusText::updateProfileUnavailable()
{
    return "Ephemeris data: Update profile unavailable";
}

QString SkyEphemerisStatusText::updateProfileMissingAsset()
{
    return "Ephemeris data: Update profile references a missing asset";
}

QString SkyEphemerisStatusText::writableCacheUnavailable()
{
    return "Ephemeris data: Writable cache unavailable";
}

QString SkyEphemerisStatusText::stagingAreaUnclearable()
{
    return "Ephemeris data: Unable to clear update staging area";
}

QString SkyEphemerisStatusText::stagingAreaUncreatable()
{
    return "Ephemeris data: Unable to create update staging area";
}

QString SkyEphemerisStatusText::checkingUpdateManifest()
{
    return "Ephemeris data: Checking update manifest...";
}

QString SkyEphemerisStatusText::downloading(const QString& progressName)
{
    return QString("Ephemeris data: Downloading %1...").arg(progressName);
}

QString SkyEphemerisStatusText::verifying(const QString& progressName)
{
    return QString("Ephemeris data: Verifying %1...").arg(progressName);
}

QString SkyEphemerisStatusText::cancelingDownload()
{
    return "Ephemeris data: Canceling download...";
}

QString SkyEphemerisStatusText::checkingSupportData()
{
    return "Ephemeris data: Checking time and Earth data...";
}

QString SkyEphemerisStatusText::supportDataUpdatesAvailable()
{
    return "Ephemeris data: Updates available for time and Earth data";
}

QString SkyEphemerisStatusText::supportDataUpToDate()
{
    return "Ephemeris data: Time and Earth data is up to date";
}

QString SkyEphemerisStatusText::failure(const QString& diagnostic)
{
    return QString("Ephemeris data: %1").arg(diagnostic);
}

}  // namespace skygate::ui::internal
