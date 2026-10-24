#include "SkyEphemerisSettingsCodec.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsValueCodecs.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"
#include "ephemeris/SkyEphemerisDataProfileIds.hpp"

#include <QSettings>

#include <array>
#include <cstdint>
#include <string>

namespace skygate::ui::internal {
namespace {

using skygate::ephemeris::EphemerisCorrectionFlags;
using skygate::ephemeris::EphemerisEngineDescriptorRegistry;
using skygate::ephemeris::EphemerisEngineKind;

// Every ephemeris setting name in one table, so saving, loading, and the
// presence check cannot drift apart.
struct EphemerisSettingKeys final {
    static constexpr const char* kEngineKind = "ephemeris/engineKind";
    static constexpr const char* kCorrectionFlags = "ephemeris/correctionFlags";
    static constexpr const char* kCorrectionPresetId = "ephemeris/correctionPresetId";
    static constexpr const char* kFallbackToSimpleEngine = "ephemeris/fallbackToSimpleEngine";
    static constexpr const char* kRefractionEnabled = "ephemeris/refractionEnabled";
    static constexpr const char* kAtmosphericPressureHpa = "ephemeris/atmosphericPressureHpa";
    static constexpr const char* kAtmosphericTemperatureC = "ephemeris/atmosphericTemperatureC";
    static constexpr const char* kRelativeHumidity = "ephemeris/relativeHumidity";
    static constexpr const char* kObservingWavelengthMicrometers = "ephemeris/observingWavelengthMicrometers";
    static constexpr const char* kPreferredDataProfileId = "ephemeris/preferredDataProfileId";
    static constexpr const char* kOnlineUpdatesEnabled = "ephemeris/onlineUpdatesEnabled";
    static constexpr const char* kUpdatePresetId = "ephemeris/updatePresetId";
    static constexpr const char* kUpdateManifestUrl = "ephemeris/updateManifestUrl";

    static constexpr std::array<const char*, 13> kAll{
        kEngineKind,
        kCorrectionFlags,
        kCorrectionPresetId,
        kFallbackToSimpleEngine,
        kRefractionEnabled,
        kAtmosphericPressureHpa,
        kAtmosphericTemperatureC,
        kRelativeHumidity,
        kObservingWavelengthMicrometers,
        kPreferredDataProfileId,
        kOnlineUpdatesEnabled,
        kUpdatePresetId,
        kUpdateManifestUrl,
    };
};

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
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kEngineKind), engineKindToString(snapshot.engineKind)
    );
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kCorrectionFlags),
        static_cast<qulonglong>(static_cast<std::uint32_t>(snapshot.correctionFlags))
    );
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kCorrectionPresetId), snapshot.correctionPresetId
    );
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kFallbackToSimpleEngine), snapshot.fallbackToSimpleEngine
    );
    settings.setValue(SkySettingsKeyBuilder::key(EphemerisSettingKeys::kRefractionEnabled), snapshot.refractionEnabled);
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kAtmosphericPressureHpa), snapshot.atmosphericPressureHpa
    );
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kAtmosphericTemperatureC), snapshot.atmosphericTemperatureC
    );
    settings.setValue(SkySettingsKeyBuilder::key(EphemerisSettingKeys::kRelativeHumidity), snapshot.relativeHumidity);
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kObservingWavelengthMicrometers),
        snapshot.observingWavelengthMicrometers
    );
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kPreferredDataProfileId), snapshot.preferredDataProfileId
    );
    settings.setValue(
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kOnlineUpdatesEnabled), snapshot.onlineUpdatesEnabled
    );
    settings.setValue(SkySettingsKeyBuilder::key(EphemerisSettingKeys::kUpdatePresetId), snapshot.updatePresetId);
    settings.setValue(SkySettingsKeyBuilder::key(EphemerisSettingKeys::kUpdateManifestUrl), snapshot.updateManifestUrl);
}

SkyEphemerisUserSettingsSnapshot SkyEphemerisSettingsCodec::load(QSettings& settings)
{
    SkyEphemerisUserSettingsSnapshot snapshot;
    const QString storedEngineKind =
        settings
            .value(
                SkySettingsKeyBuilder::key(EphemerisSettingKeys::kEngineKind), engineKindToString(snapshot.engineKind)
            )
            .toString();
    snapshot.engineKind = engineKindFromString(storedEngineKind);
    snapshot.correctionFlags = readCorrectionFlags(
        settings, SkySettingsKeyBuilder::key(EphemerisSettingKeys::kCorrectionFlags), snapshot.correctionFlags
    );
    snapshot.correctionPresetId = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, SkySettingsKeyBuilder::key(EphemerisSettingKeys::kCorrectionPresetId), snapshot.correctionPresetId
    );
    snapshot.fallbackToSimpleEngine = SkySettingsValueCodecs::readBoolSetting(
        settings,
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kFallbackToSimpleEngine),
        snapshot.fallbackToSimpleEngine
    );
    snapshot.refractionEnabled = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key(EphemerisSettingKeys::kRefractionEnabled), snapshot.refractionEnabled
    );
    snapshot.atmosphericPressureHpa = SkySettingsValueCodecs::readDoubleSetting(
        settings,
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kAtmosphericPressureHpa),
        snapshot.atmosphericPressureHpa
    );
    snapshot.atmosphericTemperatureC = SkySettingsValueCodecs::readDoubleSetting(
        settings,
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kAtmosphericTemperatureC),
        snapshot.atmosphericTemperatureC
    );
    snapshot.relativeHumidity = SkySettingsValueCodecs::readDoubleSetting(
        settings, SkySettingsKeyBuilder::key(EphemerisSettingKeys::kRelativeHumidity), snapshot.relativeHumidity
    );
    snapshot.observingWavelengthMicrometers = SkySettingsValueCodecs::readDoubleSetting(
        settings,
        SkySettingsKeyBuilder::key(EphemerisSettingKeys::kObservingWavelengthMicrometers),
        snapshot.observingWavelengthMicrometers
    );
    snapshot.preferredDataProfileId = SkyEphemerisDataProfileIds::canonical(
        SkySettingsValueCodecs::readNonBlankStringSetting(
            settings,
            SkySettingsKeyBuilder::key(EphemerisSettingKeys::kPreferredDataProfileId),
            snapshot.preferredDataProfileId
        )
    );
    snapshot.onlineUpdatesEnabled = SkySettingsValueCodecs::readBoolSetting(
        settings, SkySettingsKeyBuilder::key(EphemerisSettingKeys::kOnlineUpdatesEnabled), snapshot.onlineUpdatesEnabled
    );
    snapshot.updatePresetId = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, SkySettingsKeyBuilder::key(EphemerisSettingKeys::kUpdatePresetId), snapshot.updatePresetId
    );
    snapshot.updateManifestUrl =
        settings.value(SkySettingsKeyBuilder::key(EphemerisSettingKeys::kUpdateManifestUrl), snapshot.updateManifestUrl)
            .toString()
            .trimmed();
    return snapshot;
}

bool SkyEphemerisSettingsCodec::hasStoredSettings(QSettings& settings)
{
    for (const char* name : EphemerisSettingKeys::kAll) {
        if (settings.contains(SkySettingsKeyBuilder::key(name))) {
            return true;
        }
    }
    return false;
}

}  // namespace skygate::ui::internal
