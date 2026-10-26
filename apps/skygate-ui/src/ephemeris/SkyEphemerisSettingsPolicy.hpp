#pragma once

#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "settings/SkyEphemerisUserSettingsSnapshot.hpp"

#include <QString>
#include <QStringView>

#include <optional>

namespace skygate::ui::internal {

// Converts between the persisted user settings and engine options. Owns the
// correction-preset mapping, the engine-switch defaults, and the atmosphere
// text bounds used by the preferences surface.
class SkyEphemerisSettingsPolicy final {
public:
    static constexpr int kCorrectionPresetCount = 4;
    static constexpr QStringView kDefaultUpdateManifestUrl =
        u"https://raw.githubusercontent.com/saprykin/skygate/"
        u"high-precision-ephemeris/apps/skygate-ui/resources/ephemeris/manifest.json";

    SkyEphemerisSettingsPolicy() = delete;

    [[nodiscard]] static QString defaultWritableCacheRoot();
    [[nodiscard]] static skygate::ephemeris::EphemerisCorrectionFlags
    correctionFlagsForPreset(int presetIndex) noexcept;
    [[nodiscard]] static QString correctionPresetId(int presetIndex);
    [[nodiscard]] static int correctionPresetIndex(skygate::ephemeris::EphemerisCorrectionFlags flags) noexcept;
    [[nodiscard]] static skygate::ephemeris::EphemerisCorrectionFlags
    withRefraction(skygate::ephemeris::EphemerisCorrectionFlags flags, bool enabled) noexcept;

    [[nodiscard]] static std::optional<double> atmosphericPressureHpaFromText(const QString& text);
    [[nodiscard]] static std::optional<double> atmosphericTemperatureCFromText(const QString& text);
    [[nodiscard]] static std::optional<double> relativeHumidityFromText(const QString& text);
    [[nodiscard]] static std::optional<double> observingWavelengthMicrometersFromText(const QString& text);

    [[nodiscard]] static SkyEphemerisUserSettingsSnapshot settingsForEngineSwitch(
        SkyEphemerisUserSettingsSnapshot settings, skygate::ephemeris::EphemerisEngineKind::Type engineKind
    );
    [[nodiscard]] static SkyEphemerisUserSettingsSnapshot userSettingsFromEngine(
        SkyEphemerisUserSettingsSnapshot settings,
        skygate::ephemeris::EphemerisEngineKind::Type engineKind,
        const skygate::ephemeris::EphemerisEngineOptions& engineOptions
    );
    [[nodiscard]] static SkyEphemerisUserSettingsSnapshot
    normalizedUpdateManifestUrl(SkyEphemerisUserSettingsSnapshot settings);
    [[nodiscard]] static skygate::ephemeris::EphemerisEngineOptions engineOptionsFromSettings(
        const skygate::ephemeris::EphemerisEngineOptions& baseOptions, const SkyEphemerisUserSettingsSnapshot& settings
    );

private:
    [[nodiscard]] static std::optional<double> boundedDouble(const QString& text, double minimum, double maximum);
};

}  // namespace skygate::ui::internal
