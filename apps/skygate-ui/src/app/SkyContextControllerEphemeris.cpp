#include "SkyContextController.hpp"
#include "SkyContextTextFormatter.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "ephemeris/SkyEphemerisSettingsPolicy.hpp"
#include "ephemeris/SkyEphemerisUpdateWorkflow.hpp"
#include "factory/EphemerisFactoryCreationDiagnostic.hpp"

#include <QLoggingCategory>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace skygate::ui::internal;

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisUpdateLog, "skygate.ephemeris.update")

using skygate::ephemeris::EphemerisEngineDescriptorRegistry;
using skygate::ephemeris::EphemerisEngineKind;

[[nodiscard]] EphemerisEngineKind::Type engineKindFromIndex(const int index) noexcept
{
    if (index < 0) {
        return EphemerisEngineDescriptorRegistry::defaultDescriptor().kind;
    }
    return EphemerisEngineDescriptorRegistry::descriptorAt(static_cast<std::size_t>(index)).kind;
}

[[nodiscard]] int engineKindIndex(const EphemerisEngineKind::Type kind) noexcept
{
    const std::optional<std::size_t> index = EphemerisEngineDescriptorRegistry::indexOf(kind);
    if (index.has_value()) {
        return static_cast<int>(*index);
    }

    qCWarning(skygateEphemerisUpdateLog).noquote()
        << "Ephemeris engine kind is not registered; using the default descriptor index.";
    return 0;
}

[[nodiscard]] QString cacheSizeText(const std::uint64_t bytes)
{
    if (bytes == 0U) {
        return QStringLiteral("0 MB");
    }

    constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;
    return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / kBytesPerMegabyte, 0, 'f', 1);
}

}  // namespace

int SkyContextController::ephemerisEngineKindIndex() const noexcept
{
    return engineKindIndex(m_ephemerisSession->engineKind());
}

QStringList SkyContextController::ephemerisEngineLabels() const
{
    QStringList labels;
    labels.reserve(static_cast<qsizetype>(EphemerisEngineDescriptorRegistry::count()));
    for (std::size_t index = 0; index < EphemerisEngineDescriptorRegistry::count(); ++index) {
        labels.append(QString::fromStdString(EphemerisEngineDescriptorRegistry::descriptorAt(index).displayName));
    }
    return labels;
}

bool SkyContextController::ephemerisEngineSupportsCorrections(const int engineKindIndex) const noexcept
{
    if (engineKindIndex < 0) {
        return false;
    }
    return EphemerisEngineDescriptorRegistry::descriptorAt(static_cast<std::size_t>(engineKindIndex))
        .supportsCorrections;
}

bool SkyContextController::ephemerisEngineSupportsAtmosphereSettings(const int engineKindIndex) const noexcept
{
    if (engineKindIndex < 0) {
        return false;
    }
    return EphemerisEngineDescriptorRegistry::descriptorAt(static_cast<std::size_t>(engineKindIndex))
        .supportsAtmosphereSettings;
}

int SkyContextController::ephemerisEngineDefaultCorrectionPresetIndex(const int engineKindIndex) const noexcept
{
    if (engineKindIndex < 0) {
        return 0;
    }
    const auto& defaults =
        EphemerisEngineDescriptorRegistry::descriptorAt(static_cast<std::size_t>(engineKindIndex)).defaultOptions;
    return SkyEphemerisSettingsPolicy::correctionPresetIndex(defaults.correctionFlags());
}

bool SkyContextController::ephemerisEngineDefaultRefractionEnabled(const int engineKindIndex) const noexcept
{
    if (engineKindIndex < 0) {
        return false;
    }
    return EphemerisEngineDescriptorRegistry::descriptorAt(static_cast<std::size_t>(engineKindIndex))
        .defaultOptions.enableAtmosphericRefraction();
}

int SkyContextController::ephemerisCorrectionPresetIndex() const noexcept
{
    return SkyEphemerisSettingsPolicy::correctionPresetIndex(m_ephemerisSession->engineOptions().correctionFlags());
}

bool SkyContextController::ephemerisRefractionEnabled() const noexcept
{
    return m_ephemerisSession->engineOptions().enableAtmosphericRefraction();
}

QString SkyContextController::ephemerisAtmosphericPressureText() const
{
    return SkyContextTextFormatter::formatFixed(m_ephemerisSession->engineOptions().atmosphericPressureHpa(), 2);
}

QString SkyContextController::ephemerisAtmosphericTemperatureText() const
{
    return SkyContextTextFormatter::formatFixed(m_ephemerisSession->engineOptions().atmosphericTemperatureC(), 1);
}

QString SkyContextController::ephemerisRelativeHumidityText() const
{
    return SkyContextTextFormatter::formatFixed(m_ephemerisSession->engineOptions().relativeHumidity() * 100.0, 1);
}

QString SkyContextController::ephemerisWavelengthText() const
{
    return SkyContextTextFormatter::formatFixed(
        m_ephemerisSession->engineOptions().observingWavelengthMicrometers(), 2
    );
}

QString SkyContextController::ephemerisDataStatusText() const
{
    return m_ephemerisUpdateWorkflow->statusText();
}

QString SkyContextController::ephemerisEngineStatusText() const
{
    return m_ephemerisEngineStatusText;
}

QString SkyContextController::ephemerisShortRangeKernelStatusText() const
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->shortRangeKernelStatusText() : QString();
}

QString SkyContextController::ephemerisLongRangeKernelStatusText() const
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->longRangeKernelStatusText() : QString();
}

QString SkyContextController::ephemerisKernelDisplayText() const
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->kernelDisplayText() : QString();
}

QString SkyContextController::ephemerisEarthOrientationStatusText() const
{
    return m_ephemerisUpdateWorkflow->earthOrientationStatusText();
}

QString SkyContextController::ephemerisLeapSecondStatusText() const
{
    return m_ephemerisUpdateWorkflow->leapSecondStatusText();
}

QString SkyContextController::ephemerisDeltaTStatusText() const
{
    return m_ephemerisUpdateWorkflow->deltaTStatusText();
}

QString SkyContextController::ephemerisDataLastUpdateResultText() const
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->lastUpdateResultText() : QString();
}

QString SkyContextController::ephemerisPlanetaryKernelCacheSizeText() const
{
    return cacheSizeText(
        m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->planetaryKernelCacheSizeBytes() : 0U
    );
}

QString SkyContextController::ephemerisSupportDataCacheSizeText() const
{
    return cacheSizeText(m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->supportDataCacheSizeBytes() : 0U);
}

bool SkyContextController::ephemerisDataOnlineUpdatesEnabled() const noexcept
{
    return m_ephemerisUserSettings.onlineUpdatesEnabled;
}

bool SkyContextController::ephemerisDataUpdateEnabled() const noexcept
{
    return m_ephemerisUpdateWorkflow->updateEnabled();
}

bool SkyContextController::ephemerisDataUpdateInProgress() const noexcept
{
    return m_ephemerisUpdateWorkflow->isUpdateInProgress();
}

double SkyContextController::ephemerisDataUpdateProgress() const noexcept
{
    return m_ephemerisUpdateWorkflow->updateProgress();
}

const skygate::ephemeris::IEphemerisEngine* SkyContextController::ephemerisEngine() const noexcept
{
    return m_ephemerisSession->engine();
}

std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> SkyContextController::ephemerisEngineHandle() const
{
    return m_ephemerisSession->engineHandle();
}

std::shared_ptr<skygate::ephemeris::IEphemerisGuidanceStrategy>
SkyContextController::ephemerisGuidanceStrategy() const noexcept
{
    return m_ephemerisSession->guidanceStrategy();
}

std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot>
SkyContextController::activeEphemerisDataSnapshot() const noexcept
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->activeDataSnapshot() : nullptr;
}

std::uint64_t SkyContextController::ephemerisDataRevision() const noexcept
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->dataRevision() : 0U;
}

SkyContextController::EphemerisRequestContext SkyContextController::ephemerisRequestContext() const
{
    return ephemerisRequestContextFor(skyContext());
}

SkyContextController::EphemerisRequestContext
SkyContextController::ephemerisRequestContextFor(const skygate::core::ObservationContext& skyContext) const
{
    SkyEphemerisSession::RequestInputs inputs;
    inputs.observationContext = skyContext;
    inputs.activeDataSnapshot = activeEphemerisDataSnapshot();
    inputs.dataManifest = activeEphemerisDataManifest();
    inputs.ephemerisDataRevision = ephemerisDataRevision();
    inputs.catalogRevision = catalogRevision();
    if (m_ephemerisDataManager != nullptr) {
        const SkyEphemerisDataCacheSnapshot activeCache = m_ephemerisDataManager->activeCacheSnapshot();
        inputs.installedEarthOrientationVersion = activeCache.installedEarthOrientationVersion;
        inputs.installedLeapSecondTableVersion = activeCache.installedLeapSecondTableVersion;
        inputs.installedDeltaTDataVersion = activeCache.installedDeltaTDataVersion;
    }

    return m_ephemerisSession->requestContext(inputs);
}

void SkyContextController::rebuildEphemerisEngine()
{
    m_ephemerisSession->rebuild(
        SkyEphemerisSession::RebuildInputs{
            .starCatalog = m_catalogManager != nullptr ? m_catalogManager->starCatalog() : nullptr,
            .activeDataSnapshot = activeEphemerisDataSnapshot(),
            .dataManifest = activeEphemerisDataManifest(),
            .ephemerisDataRevision = ephemerisDataRevision(),
        },
        [this](const std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic>& diagnostics) {
            QStringList diagnosticsText;
            for (const skygate::ephemeris::EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
                diagnosticsText.append(QString::fromStdString(std::string{diagnostic.displayText()}));
            }
            setEphemerisEngineStatusText(diagnosticsText.join(QStringLiteral("; ")));
        }
    );
}

void SkyContextController::applyEphemerisUserSettings(const SkyEphemerisUserSettingsSnapshot& settings)
{
    const skygate::ephemeris::EphemerisEngineOptions nextOptions =
        SkyEphemerisSettingsPolicy::engineOptionsFromSettings(m_ephemerisSession->engineOptions(), settings);

    m_ephemerisUserSettings = SkyEphemerisSettingsPolicy::normalizedUpdateManifestUrl(settings);
    if (!m_ephemerisSession->applyOptions(settings.engineKind, nextOptions)) {
        return;
    }

    rebuildEphemerisEngine();
    emit ephemerisSettingsChanged();
    emit skyContextChanged();
}

void SkyContextController::setEphemerisEngineKindIndex(const int engineKindIndex)
{
    if (engineKindIndex < 0
        || static_cast<std::size_t>(engineKindIndex) >= EphemerisEngineDescriptorRegistry::count()) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    const EphemerisEngineKind::Type targetKind = engineKindFromIndex(engineKindIndex);
    if (targetKind != m_ephemerisSession->engineKind()) {
        settings = SkyEphemerisSettingsPolicy::settingsForEngineSwitch(std::move(settings), targetKind);
    } else {
        settings.engineKind = targetKind;
    }
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisCorrectionPresetIndex(const int correctionPresetIndex)
{
    if (correctionPresetIndex < 0 || correctionPresetIndex >= SkyEphemerisSettingsPolicy::kCorrectionPresetCount) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.correctionPresetId = SkyEphemerisSettingsPolicy::correctionPresetId(correctionPresetIndex);
    settings.correctionFlags = SkyEphemerisSettingsPolicy::withRefraction(
        SkyEphemerisSettingsPolicy::correctionFlagsForPreset(correctionPresetIndex), settings.refractionEnabled
    );
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisRefractionEnabled(const bool enabled)
{
    auto settings = m_ephemerisUserSettings;
    settings.refractionEnabled = enabled;
    settings.correctionFlags = SkyEphemerisSettingsPolicy::withRefraction(
        SkyEphemerisSettingsPolicy::correctionFlagsForPreset(ephemerisCorrectionPresetIndex()), enabled
    );
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisAtmosphericPressureText(const QString& pressureText)
{
    const std::optional<double> pressure = SkyEphemerisSettingsPolicy::atmosphericPressureHpaFromText(pressureText);
    if (!pressure.has_value()) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.atmosphericPressureHpa = *pressure;
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisAtmosphericTemperatureText(const QString& temperatureText)
{
    const std::optional<double> temperature =
        SkyEphemerisSettingsPolicy::atmosphericTemperatureCFromText(temperatureText);
    if (!temperature.has_value()) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.atmosphericTemperatureC = *temperature;
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisRelativeHumidityText(const QString& humidityText)
{
    const std::optional<double> humidity = SkyEphemerisSettingsPolicy::relativeHumidityFromText(humidityText);
    if (!humidity.has_value()) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.relativeHumidity = *humidity;
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisWavelengthText(const QString& wavelengthText)
{
    const std::optional<double> wavelength =
        SkyEphemerisSettingsPolicy::observingWavelengthMicrometersFromText(wavelengthText);
    if (!wavelength.has_value()) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.observingWavelengthMicrometers = *wavelength;
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisDataOnlineUpdatesEnabled(const bool enabled)
{
    if (m_ephemerisUserSettings.onlineUpdatesEnabled == enabled) {
        return;
    }

    m_ephemerisUserSettings.onlineUpdatesEnabled = enabled;
    emit ephemerisDataStatusTextChanged();
}

void SkyContextController::setEphemerisEngineStatusText(QString statusText)
{
    statusText = statusText.trimmed();
    if (m_ephemerisEngineStatusText == statusText) {
        return;
    }

    m_ephemerisEngineStatusText = std::move(statusText);
    emit ephemerisEngineStatusTextChanged();
}

const skygate::ephemeris::EphemerisDataManifest* SkyContextController::activeEphemerisDataManifest() const noexcept
{
    return m_ephemerisUpdateWorkflow->activeManifest();
}

bool SkyContextController::clearEphemerisDataCache()
{
    return m_ephemerisUpdateWorkflow->clearInstalledDataCache();
}

bool SkyContextController::clearPlanetaryKernelCache()
{
    return m_ephemerisUpdateWorkflow->clearPlanetaryKernelCache();
}

bool SkyContextController::clearSupportDataCache()
{
    return m_ephemerisUpdateWorkflow->clearSupportDataCache();
}

bool SkyContextController::checkEphemerisSupportDataUpdates()
{
    return m_ephemerisUpdateWorkflow->checkSupportDataUpdates(m_ephemerisUserSettings.updateManifestUrl);
}

void SkyContextController::cancelActiveDownload()
{
    m_ephemerisUpdateWorkflow->cancelActiveUpdate();
    if (m_catalogManager != nullptr && m_catalogManager->downloadingCatalog()) {
        m_catalogManager->cancelCatalogDownload();
    }
}

bool SkyContextController::updateEphemerisData()
{
    return updateEphemerisDataProfile(m_ephemerisUserSettings.preferredDataProfileId);
}

bool SkyContextController::updateEphemerisDataProfile(const QString& profileIdText)
{
    return m_ephemerisUpdateWorkflow->updateProfile(profileIdText, m_ephemerisUserSettings.updateManifestUrl);
}
