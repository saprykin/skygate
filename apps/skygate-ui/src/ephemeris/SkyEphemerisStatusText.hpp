#pragma once

#include <QString>

#include <string_view>

namespace skygate::ui::internal {

// Status texts of the installed ephemeris data and its update workflow. The
// wording and the status prefix live here instead of at the call sites, so the
// preferences footer, the progress reports, and the failure messages agree.
class SkyEphemerisStatusText final {
public:
    // Prefix carried by every ephemeris data status text. Places that present a
    // status in their own context drop it from the rendered text.
    static constexpr std::string_view kStatusPrefix = "Ephemeris data: ";

    [[nodiscard]] static QString bundledFallback();
    [[nodiscard]] static QString installedDataMissing();
    [[nodiscard]] static QString installedDataActive();

    [[nodiscard]] static QString updateAlreadyInProgress();
    [[nodiscard]] static QString updateManifestUnavailable();
    [[nodiscard]] static QString updateManifestUnreadable();
    [[nodiscard]] static QString updateManifestTooLarge();
    [[nodiscard]] static QString updateManagerUnavailable();
    [[nodiscard]] static QString updateProfileUnavailable();
    [[nodiscard]] static QString updateProfileMissingAsset();
    [[nodiscard]] static QString writableCacheUnavailable();
    [[nodiscard]] static QString stagingAreaUnclearable();
    [[nodiscard]] static QString stagingAreaUncreatable();
    [[nodiscard]] static QString checkingUpdateManifest();
    [[nodiscard]] static QString downloading(const QString& progressName);
    [[nodiscard]] static QString verifying(const QString& progressName);
    [[nodiscard]] static QString cancelingDownload();
    [[nodiscard]] static QString checkingSupportData();
    [[nodiscard]] static QString supportDataUpdatesAvailable();
    [[nodiscard]] static QString supportDataUpToDate();
    [[nodiscard]] static QString failure(const QString& diagnostic);
};

}  // namespace skygate::ui::internal
