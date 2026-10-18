#include "SkyEphemerisSettingsPolicy.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"

#include <QString>

#include <optional>

namespace {

constexpr double kMinimumPressureHpa = 0.0;
constexpr double kMaximumPressureHpa = 2000.0;
constexpr double kMinimumTemperatureC = -100.0;
constexpr double kMaximumTemperatureC = 80.0;
constexpr double kMinimumRelativeHumidityPercent = 0.0;
constexpr double kMaximumRelativeHumidityPercent = 100.0;
constexpr double kMinimumWavelengthMicrometers = 0.1;
constexpr double kMaximumWavelengthMicrometers = 100.0;

}  // namespace

namespace skygate::ui::internal {

skygate::ephemeris::EphemerisCorrectionFlags
SkyEphemerisSettingsPolicy::correctionFlagsForPreset(const int presetIndex) noexcept
{
    switch (presetIndex) {
    case 0:
        return skygate::ephemeris::EphemerisCorrectionFlags::geometric();
    case 1:
        return skygate::ephemeris::EphemerisCorrectionFlags::astrometric();
    case 2:
        return skygate::ephemeris::EphemerisCorrectionFlags::apparent();
    default:
        return skygate::ephemeris::EphemerisCorrectionFlags::topocentric();
    }
}

QString SkyEphemerisSettingsPolicy::correctionPresetId(const int presetIndex)
{
    switch (presetIndex) {
    case 0:
        return QStringLiteral("geometric");
    case 1:
        return QStringLiteral("astrometric");
    case 2:
        return QStringLiteral("apparent");
    default:
        return QStringLiteral("apparent-topocentric");
    }
}

int SkyEphemerisSettingsPolicy::correctionPresetIndex(const skygate::ephemeris::EphemerisCorrectionFlags flags) noexcept
{
    const skygate::ephemeris::EphemerisCorrectionFlags baseFlags =
        skygate::ephemeris::EphemerisCorrectionFlags::without(
            flags, skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
        );
    if (baseFlags == skygate::ephemeris::EphemerisCorrectionFlags::geometric()
        || baseFlags == skygate::ephemeris::EphemerisCorrectionFlags::noCorrections()) {
        return 0;
    }
    if (baseFlags == skygate::ephemeris::EphemerisCorrectionFlags::astrometric()) {
        return 1;
    }
    if (baseFlags == skygate::ephemeris::EphemerisCorrectionFlags::apparent()) {
        return 2;
    }
    return 3;
}

skygate::ephemeris::EphemerisCorrectionFlags SkyEphemerisSettingsPolicy::withRefraction(
    const skygate::ephemeris::EphemerisCorrectionFlags flags, const bool enabled
) noexcept
{
    if (enabled) {
        return flags | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction();
    }
    return skygate::ephemeris::EphemerisCorrectionFlags::without(
        flags, skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    );
}

std::optional<double> SkyEphemerisSettingsPolicy::atmosphericPressureHpaFromText(const QString& text)
{
    return boundedDouble(text, kMinimumPressureHpa, kMaximumPressureHpa);
}

std::optional<double> SkyEphemerisSettingsPolicy::atmosphericTemperatureCFromText(const QString& text)
{
    return boundedDouble(text, kMinimumTemperatureC, kMaximumTemperatureC);
}

std::optional<double> SkyEphemerisSettingsPolicy::relativeHumidityFromText(const QString& text)
{
    const std::optional<double> humidityPercent =
        boundedDouble(text, kMinimumRelativeHumidityPercent, kMaximumRelativeHumidityPercent);
    if (!humidityPercent.has_value()) {
        return std::nullopt;
    }
    return *humidityPercent / 100.0;
}

std::optional<double> SkyEphemerisSettingsPolicy::observingWavelengthMicrometersFromText(const QString& text)
{
    return boundedDouble(text, kMinimumWavelengthMicrometers, kMaximumWavelengthMicrometers);
}

SkyEphemerisUserSettingsSnapshot SkyEphemerisSettingsPolicy::settingsForEngineSwitch(
    SkyEphemerisUserSettingsSnapshot settings, const skygate::ephemeris::EphemerisEngineKind::Type engineKind
)
{
    settings.engineKind = engineKind;
    const auto* descriptor = skygate::ephemeris::EphemerisEngineDescriptorRegistry::findByKind(engineKind);
    if (descriptor == nullptr) {
        return settings;
    }

    const skygate::ephemeris::EphemerisEngineOptions& defaults = descriptor->defaultOptions;
    settings.correctionFlags = defaults.correctionFlags();
    settings.refractionEnabled = defaults.enableAtmosphericRefraction();
    settings.correctionPresetId = correctionPresetId(correctionPresetIndex(defaults.correctionFlags()));
    return settings;
}

SkyEphemerisUserSettingsSnapshot SkyEphemerisSettingsPolicy::userSettingsFromEngine(
    SkyEphemerisUserSettingsSnapshot settings,
    const skygate::ephemeris::EphemerisEngineKind::Type engineKind,
    const skygate::ephemeris::EphemerisEngineOptions& engineOptions
)
{
    settings.engineKind = engineKind;
    settings.correctionFlags = engineOptions.correctionFlags();
    settings.fallbackToSimpleEngine = engineOptions.fallbackToSimpleEngine();
    settings.refractionEnabled = engineOptions.enableAtmosphericRefraction();
    settings.atmosphericPressureHpa = engineOptions.atmosphericPressureHpa();
    settings.atmosphericTemperatureC = engineOptions.atmosphericTemperatureC();
    settings.relativeHumidity = engineOptions.relativeHumidity();
    settings.observingWavelengthMicrometers = engineOptions.observingWavelengthMicrometers();
    return settings;
}

skygate::ephemeris::EphemerisEngineOptions SkyEphemerisSettingsPolicy::engineOptionsFromSettings(
    const skygate::ephemeris::EphemerisEngineOptions& baseOptions, const SkyEphemerisUserSettingsSnapshot& settings
)
{
    skygate::ephemeris::EphemerisEngineOptions engineOptions = baseOptions;
    engineOptions.setEngineKind(settings.engineKind);
    engineOptions.setCorrectionFlags(settings.correctionFlags);
    engineOptions.setFallbackToSimpleEngine(settings.fallbackToSimpleEngine);
    engineOptions.setEnableAtmosphericRefraction(settings.refractionEnabled);
    engineOptions.setAtmosphericPressureHpa(settings.atmosphericPressureHpa);
    engineOptions.setAtmosphericTemperatureC(settings.atmosphericTemperatureC);
    engineOptions.setRelativeHumidity(settings.relativeHumidity);
    engineOptions.setObservingWavelengthMicrometers(settings.observingWavelengthMicrometers);
    return engineOptions;
}

std::optional<double>
SkyEphemerisSettingsPolicy::boundedDouble(const QString& text, const double minimum, const double maximum)
{
    bool isValidNumber = false;
    const double value = text.trimmed().toDouble(&isValidNumber);
    if (!isValidNumber || value < minimum || value > maximum) {
        return std::nullopt;
    }
    return value;
}

}  // namespace skygate::ui::internal
