#include "SkyEphemerisSettingsCodec.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsValueCodecs.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"
#include "ephemeris/SkyEphemerisDataProfileIds.hpp"

#include <QSettings>

#include <cstdint>
#include <string>

namespace skygate::ui::internal {
namespace {

using skygate::ephemeris::EphemerisCorrectionFlags;
using skygate::ephemeris::EphemerisEngineDescriptorRegistry;
using skygate::ephemeris::EphemerisEngineKind;

QString ephemerisSettingsKey(const char* name)
{
    return SkySettingsKeyBuilder::key(QString::fromUtf8(name));
}

[[nodiscard]] QString engineKindToString(const EphemerisEngineKind::Type engineKind)
{
    const auto* descriptor = EphemerisEngineDescriptorRegistry::findByKind(engineKind);
    const auto& resolved = descriptor != nullptr ? *descriptor : EphemerisEngineDescriptorRegistry::defaultDescriptor();
    return QString::fromStdString(resolved.id);
}

[[nodiscard]] EphemerisEngineKind::Type engineKindFromString(const QString& text)
{
    const std::string normalizedText = EphemerisEngineDescriptorRegistry::normalizedLookupKey(text.toStdString());
    const auto* descriptor = EphemerisEngineDescriptorRegistry::findByIdIgnoreAsciiCase(normalizedText);
    if (descriptor != nullptr) {
        return descriptor->kind;
    }

    if (normalizedText == "high-precision") {
        return EphemerisEngineKind::Type::HighPrecision;
    }

    return EphemerisEngineDescriptorRegistry::defaultDescriptor().kind;
}

[[nodiscard]] EphemerisCorrectionFlags
readCorrectionFlags(QSettings& settings, const QString& key, const EphemerisCorrectionFlags fallback)
{
    constexpr std::uint32_t kSupportedCorrectionMask =
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::apparentTopocentric());
    const qulonglong rawFlags = SkySettingsValueCodecs::readULongLongSetting(
        settings, key, static_cast<qulonglong>(static_cast<std::uint32_t>(fallback))
    );
    if ((rawFlags & ~static_cast<qulonglong>(kSupportedCorrectionMask)) != 0ULL) {
        return fallback;
    }
    return static_cast<EphemerisCorrectionFlags>(static_cast<std::uint32_t>(rawFlags));
}

}  // namespace

void SkyEphemerisSettingsCodec::save(QSettings& settings, const SkyEphemerisUserSettingsSnapshot& snapshot)
{
    settings.setValue(ephemerisSettingsKey("ephemeris/engineKind"), engineKindToString(snapshot.engineKind));
    settings.setValue(
        ephemerisSettingsKey("ephemeris/correctionFlags"),
        static_cast<qulonglong>(static_cast<std::uint32_t>(snapshot.correctionFlags))
    );
    settings.setValue(ephemerisSettingsKey("ephemeris/correctionPresetId"), snapshot.correctionPresetId);
    settings.setValue(ephemerisSettingsKey("ephemeris/fallbackToSimpleEngine"), snapshot.fallbackToSimpleEngine);
    settings.setValue(ephemerisSettingsKey("ephemeris/refractionEnabled"), snapshot.refractionEnabled);
    settings.setValue(ephemerisSettingsKey("ephemeris/atmosphericPressureHpa"), snapshot.atmosphericPressureHpa);
    settings.setValue(ephemerisSettingsKey("ephemeris/atmosphericTemperatureC"), snapshot.atmosphericTemperatureC);
    settings.setValue(ephemerisSettingsKey("ephemeris/relativeHumidity"), snapshot.relativeHumidity);
    settings.setValue(
        ephemerisSettingsKey("ephemeris/observingWavelengthMicrometers"), snapshot.observingWavelengthMicrometers
    );
    settings.setValue(ephemerisSettingsKey("ephemeris/preferredDataProfileId"), snapshot.preferredDataProfileId);
    settings.setValue(ephemerisSettingsKey("ephemeris/onlineUpdatesEnabled"), snapshot.onlineUpdatesEnabled);
    settings.setValue(ephemerisSettingsKey("ephemeris/updatePresetId"), snapshot.updatePresetId);
    settings.setValue(ephemerisSettingsKey("ephemeris/updateManifestUrl"), snapshot.updateManifestUrl);
}

SkyEphemerisUserSettingsSnapshot SkyEphemerisSettingsCodec::load(QSettings& settings)
{
    SkyEphemerisUserSettingsSnapshot snapshot;
    snapshot.engineKind = engineKindFromString(
        settings.value(ephemerisSettingsKey("ephemeris/engineKind"), engineKindToString(snapshot.engineKind)).toString()
    );
    snapshot.correctionFlags =
        readCorrectionFlags(settings, ephemerisSettingsKey("ephemeris/correctionFlags"), snapshot.correctionFlags);
    snapshot.correctionPresetId = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, ephemerisSettingsKey("ephemeris/correctionPresetId"), snapshot.correctionPresetId
    );
    snapshot.fallbackToSimpleEngine = SkySettingsValueCodecs::readBoolSetting(
        settings, ephemerisSettingsKey("ephemeris/fallbackToSimpleEngine"), snapshot.fallbackToSimpleEngine
    );
    snapshot.refractionEnabled = SkySettingsValueCodecs::readBoolSetting(
        settings, ephemerisSettingsKey("ephemeris/refractionEnabled"), snapshot.refractionEnabled
    );
    snapshot.atmosphericPressureHpa = SkySettingsValueCodecs::readDoubleSetting(
        settings, ephemerisSettingsKey("ephemeris/atmosphericPressureHpa"), snapshot.atmosphericPressureHpa
    );
    snapshot.atmosphericTemperatureC = SkySettingsValueCodecs::readDoubleSetting(
        settings, ephemerisSettingsKey("ephemeris/atmosphericTemperatureC"), snapshot.atmosphericTemperatureC
    );
    snapshot.relativeHumidity = SkySettingsValueCodecs::readDoubleSetting(
        settings, ephemerisSettingsKey("ephemeris/relativeHumidity"), snapshot.relativeHumidity
    );
    snapshot.observingWavelengthMicrometers = SkySettingsValueCodecs::readDoubleSetting(
        settings,
        ephemerisSettingsKey("ephemeris/observingWavelengthMicrometers"),
        snapshot.observingWavelengthMicrometers
    );
    snapshot.preferredDataProfileId = SkyEphemerisDataProfileIds::canonical(
        SkySettingsValueCodecs::readNonBlankStringSetting(
            settings, ephemerisSettingsKey("ephemeris/preferredDataProfileId"), snapshot.preferredDataProfileId
        )
    );
    snapshot.onlineUpdatesEnabled = SkySettingsValueCodecs::readBoolSetting(
        settings, ephemerisSettingsKey("ephemeris/onlineUpdatesEnabled"), snapshot.onlineUpdatesEnabled
    );
    snapshot.updatePresetId = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, ephemerisSettingsKey("ephemeris/updatePresetId"), snapshot.updatePresetId
    );
    snapshot.updateManifestUrl =
        settings.value(ephemerisSettingsKey("ephemeris/updateManifestUrl"), snapshot.updateManifestUrl)
            .toString()
            .trimmed();
    return snapshot;
}

bool SkyEphemerisSettingsCodec::hasStoredSettings(QSettings& settings)
{
    return settings.contains(ephemerisSettingsKey("ephemeris/engineKind"))
           || settings.contains(ephemerisSettingsKey("ephemeris/correctionFlags"))
           || settings.contains(ephemerisSettingsKey("ephemeris/correctionPresetId"))
           || settings.contains(ephemerisSettingsKey("ephemeris/fallbackToSimpleEngine"))
           || settings.contains(ephemerisSettingsKey("ephemeris/refractionEnabled"))
           || settings.contains(ephemerisSettingsKey("ephemeris/atmosphericPressureHpa"))
           || settings.contains(ephemerisSettingsKey("ephemeris/atmosphericTemperatureC"))
           || settings.contains(ephemerisSettingsKey("ephemeris/relativeHumidity"))
           || settings.contains(ephemerisSettingsKey("ephemeris/observingWavelengthMicrometers"))
           || settings.contains(ephemerisSettingsKey("ephemeris/preferredDataProfileId"))
           || settings.contains(ephemerisSettingsKey("ephemeris/onlineUpdatesEnabled"))
           || settings.contains(ephemerisSettingsKey("ephemeris/updatePresetId"))
           || settings.contains(ephemerisSettingsKey("ephemeris/updateManifestUrl"));
}

}  // namespace skygate::ui::internal
