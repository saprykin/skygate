#include "SkyContextController.hpp"
#include "SkyContextLocationSource.hpp"
#include "SkyContextLocationSourceCodec.hpp"
#include "SkyContextProjectionTypeCodec.hpp"
#include "SkyContextTextFormatter.hpp"
#include "SkyContextTimeCodec.hpp"
#include "SkyContextUtcDateTimeTextCodec.hpp"
#include "SkySelectedCityChange.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogSourceCollectionModel.hpp"
#include "catalog/SkyCatalogSourcePresetModel.hpp"
#include "diagnostics/SkyLogging.hpp"
#include "diagnostics/SkyLoggingConfiguration.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisEngineDescriptorRegistry.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "ephemeris/SkyEphemerisSettingsPolicy.hpp"
#include "ephemeris/SkyEphemerisUpdateWorkflow.hpp"
#include "location/LocationCatalogModel.hpp"
#include "location/SkyDeviceLocationAcquisition.hpp"
#include "projections/ProjectionTypeCodec.hpp"
#include "search/SkyObjectSearchModel.hpp"
#include "settings/SkySettingsStore.hpp"
#include "theme/SkyOverlayLayerSettings.hpp"
#include "theme/SkyThemeDefinition.hpp"
#include "theme/SkyThemePalette.hpp"
#include "theme/SkyThemeRenderPalette.hpp"
#include "theme/SkyThemeRepository.hpp"
#include "time/SkyQtTimeCodec.hpp"
#include "time/SkyTimeController.hpp"

#include <QDateTime>
#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#endif
#include <QLoggingCategory>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace skygate::ui::internal;

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisUpdateLog, "skygate.ephemeris.update")

constexpr int kUtcTickIntervalMs = 1000;

using skygate::ephemeris::EphemerisEngineDescriptorRegistry;
using skygate::ephemeris::EphemerisEngineKind;

[[nodiscard]] QVector<SkyCatalogSourceDescriptor> combinedSourcePresetDescriptors()
{
    QVector<SkyCatalogSourceDescriptor> descriptors;
    for (const SkyCatalogSourceDescriptor& descriptor : SkyCatalogPresets::starSourceDescriptors()) {
        if (!descriptor.bundled) {
            descriptors.push_back(descriptor);
        }
    }
    for (const SkyCatalogSourceDescriptor& descriptor : SkyCatalogPresets::deepSkySourceDescriptors()) {
        if (!descriptor.bundled) {
            descriptors.push_back(descriptor);
        }
    }
    return descriptors;
}

[[nodiscard]] QVector<SkyCatalogSourcePresetModel::CustomOption> combinedCustomSourceOptions()
{
    return {
        {QStringLiteral("Custom star catalog URL"), SkyCatalogPresets::starCategory()},
        {QStringLiteral("Custom deep-sky catalog URL"), SkyCatalogPresets::deepSkyCategory()},
    };
}

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

SkyContextController::SkyContextController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    QObject* parent
)
    : SkyContextController(
          std::move(starCatalog),
          std::move(ephemerisEngine),
          InitializationOptions{.loadSettings = true, .initializeLocation = true},
          parent
      )
{
}

SkyContextController::SkyContextController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    InitializationOptions initializationOptions,
    QObject* parent
)
    : QObject(parent),
      m_timeSource(
          initializationOptions.timeSource != nullptr ? initializationOptions.timeSource : &m_systemTimeSource
      ),
      m_deviceLocationAcquisition(std::make_unique<SkyDeviceLocationAcquisition>(this)),
      m_locationCatalogModel(std::make_unique<LocationCatalogModel>(this)),
      m_themePalette(std::make_unique<SkyThemePalette>(this)),
      m_themeRepository(std::make_unique<SkyThemeRepository>()),
      m_timeController(std::make_unique<SkyTimeController>(*m_timeSource, this)),
      m_overlayLayerSettings(std::make_unique<SkyOverlayLayerSettings>(this)),
      m_settingsStore(std::make_unique<SkySettingsStore>()),
      m_ephemerisDataManager(std::make_unique<SkyEphemerisDataManager>(m_settingsStore.get(), this)),
      m_ephemerisSession(
          std::make_unique<SkyEphemerisSession>(
              std::shared_ptr<const skygate::ephemeris::IEphemerisEngine>(std::move(ephemerisEngine)),
              SkyEphemerisSession::FactoryInputs{
                  .datasetManifest = initializationOptions.ephemerisFactoryInputs.datasetManifest,
                  .timeScaleService = std::move(initializationOptions.ephemerisFactoryInputs.timeScaleService),
                  .earthOrientationProvider =
                      std::move(initializationOptions.ephemerisFactoryInputs.earthOrientationProvider),
                  .calcephKernelProvider =
                      std::move(initializationOptions.ephemerisFactoryInputs.calcephKernelProvider),
                  .diagnosticsSink = initializationOptions.ephemerisFactoryInputs.diagnosticsSink,
              }
          )
      ),
      m_catalogManager(std::make_unique<SkyCatalogManager>(m_settingsStore.get(), std::move(starCatalog), this)),
      m_objectSearchModel(std::make_unique<SkyObjectSearchModel>(this)),
      m_catalogSourcePresetModel(
          std::make_unique<SkyCatalogSourcePresetModel>(
              combinedSourcePresetDescriptors(), combinedCustomSourceOptions(), this
          )
      ),
      m_catalogSourceCollectionModel(std::make_unique<SkyCatalogSourceCollectionModel>(this))
{
    m_logFilePath = skygate::ui::SkyLogging::defaultLogFilePath();
    if (initializationOptions.positionSource != nullptr) {
        m_deviceLocationAcquisition->setPositionSource(initializationOptions.positionSource);
    }
    m_deviceLocationAcquisition->setPermissionRequestsEnabled(initializationOptions.requestLocationPermission);
    connect(
        m_deviceLocationAcquisition.get(),
        &SkyDeviceLocationAcquisition::statusTextChanged,
        this,
        [this](const QString& locationStatusText) {
            if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
                return;
            }

            setLocationStatusText(locationStatusText);
        }
    );
#if SKYGATE_HAS_POSITIONING
    connect(
        m_deviceLocationAcquisition.get(),
        &SkyDeviceLocationAcquisition::positionAcquired,
        this,
        &SkyContextController::applyCurrentLocation
    );
#endif
    m_ephemerisUserSettings = SkyEphemerisSettingsPolicy::normalizedUpdateManifestUrl(
        SkyEphemerisSettingsPolicy::userSettingsFromEngine(
            m_ephemerisUserSettings, m_ephemerisSession->engineKind(), m_ephemerisSession->engineOptions()
        )
    );
    if (m_ephemerisDataManager != nullptr) {
        m_ephemerisDataManager->setBundledFallbackData(
            initializationOptions.ephemerisFactoryInputs.dataManifest,
            initializationOptions.ephemerisFactoryInputs.updateResourceRoot
        );
    }
    m_ephemerisUpdateWorkflow = std::make_unique<SkyEphemerisUpdateWorkflow>(
        m_ephemerisDataManager.get(),
        initializationOptions.ephemerisFactoryInputs.dataManifest,
        initializationOptions.ephemerisFactoryInputs.updateResourceRoot,
        initializationOptions.ephemerisFactoryInputs.writableCacheRoot,
        this
    );
    connect(
        m_ephemerisUpdateWorkflow.get(),
        &SkyEphemerisUpdateWorkflow::statusChanged,
        this,
        &SkyContextController::ephemerisDataStatusTextChanged
    );
    connect(
        m_ephemerisUpdateWorkflow.get(),
        &SkyEphemerisUpdateWorkflow::preferredProfileSelected,
        this,
        [this](const QString& profileId) { m_ephemerisUserSettings.preferredDataProfileId = profileId; }
    );
    if (initializationOptions.rebuildEphemerisEngineOnStartup) {
        rebuildEphemerisEngine();
    }

    m_themeOptions = m_themeRepository->themeOptions();
    m_themePalette->setDefinition(m_themeRepository->defaultTheme());
    connect(
        m_overlayLayerSettings.get(),
        &SkyOverlayLayerSettings::visibilityChanged,
        this,
        &SkyContextController::skyContextChanged
    );
    connect(
        m_timeController.get(),
        &SkyTimeController::utcDateTimeChangeRequested,
        this,
        [this](const QDateTime& utcDateTime) {
            setCurrentUtc(utcDateTime);
            setLive(false);
        }
    );
    connect(m_timeController.get(), &SkyTimeController::goLiveNowRequested, this, &SkyContextController::goLiveNow);
    connect(
        m_timeController.get(), &SkyTimeController::timeZoneChanged, this, &SkyContextController::refreshNightConditions
    );
    connect(
        m_ephemerisDataManager.get(),
        &SkyEphemerisDataManager::statusTextChanged,
        this,
        &SkyContextController::ephemerisDataStatusTextChanged
    );
    connect(m_ephemerisDataManager.get(), &SkyEphemerisDataManager::activeDataChanged, this, [this] {
        rebuildEphemerisEngine();
        emit ephemerisDataStatusTextChanged();
        emit ephemerisDataChanged();
        emit skyContextChanged();
    });

    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::statusTextChanged,
        this,
        &SkyContextController::catalogStatusTextChanged
    );
    // The participation summary is part of the catalog status text, so every
    // status update is also a summary update.
    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::statusTextChanged,
        this,
        &SkyContextController::catalogParticipationSummaryChanged
    );
    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::datasetInfoTextChanged,
        this,
        &SkyContextController::catalogDatasetInfoTextChanged
    );
    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::deepSkyCatalogInfoTextChanged,
        this,
        &SkyContextController::deepSkyCatalogInfoTextChanged
    );
    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::downloadingCatalogChanged,
        this,
        &SkyContextController::downloadingCatalogChanged
    );
    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::catalogProcessingChanged,
        this,
        &SkyContextController::catalogProcessingChanged
    );
    connect(m_catalogManager.get(), &SkyCatalogManager::catalogChanged, this, [this] {
        rebuildEphemerisEngine();
        refreshObjectSearchModel();
        emit nightConditionsChanged();
        if (!recenterTrackedTarget(true)) {
            emit skyContextChanged();
        }
    });
    connect(m_catalogManager.get(), &SkyCatalogManager::sourcesChanged, this, [this] {
        refreshCatalogSourceCollectionModel();
    });
    refreshObjectSearchModel();
    refreshCatalogSourceCollectionModel();

    m_location.setUtcTime(m_timeSource->nowUtc());
    m_timeController->setUtcTimePoint(m_location.utcTime());
    if (initializationOptions.loadSettings) {
        loadSettings();
    }
    if (m_timeline.live() && !m_liveClock.isRunning()) {
        m_liveClock.start(m_location.utcTime());
    }
    updateLocationStatusText();

    m_timer.setInterval(kUtcTickIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SkyContextController::tickUtcTime);
    m_timer.start();

    if (initializationOptions.initializeLocation && m_location.source() == SkyContextLocationSource::CurrentDevice) {
        m_deviceLocationAcquisition->start();
    }
}

SkyContextController::~SkyContextController() = default;

bool SkyContextController::live() const noexcept
{
    return m_timeline.live();
}

QString SkyContextController::selectedSearchTargetKind() const
{
    return m_search.selectedTargetKind();
}

QString SkyContextController::selectedSearchTargetId() const
{
    return m_search.selectedTargetId();
}

bool SkyContextController::hasTrackedTarget() const
{
    return m_search.hasTrackedTarget();
}

QString SkyContextController::trackedTargetKind() const
{
    return m_search.trackedTargetKind();
}

QString SkyContextController::trackedTargetId() const
{
    return m_search.trackedTargetId();
}

QString SkyContextController::trackedTargetDisplayText() const
{
    return m_search.trackedTargetDisplayText();
}

QVariantMap SkyContextController::nightConditions() const
{
    return m_nightConditions;
}

double SkyContextController::speedMultiplier() const noexcept
{
    return m_timeline.speedMultiplier();
}

int SkyContextController::stepSeconds() const noexcept
{
    return m_timeline.stepSeconds();
}

double SkyContextController::magnitudeCutoff() const noexcept
{
    return m_view.magnitudeCutoff();
}

double SkyContextController::viewCenterAltitudeDeg() const noexcept
{
    return m_view.centerAltitudeDeg();
}

double SkyContextController::viewCenterAzimuthDeg() const noexcept
{
    return m_view.centerAzimuthDeg();
}

double SkyContextController::viewFieldOfViewDeg() const noexcept
{
    return m_view.fieldOfViewDeg();
}

skygate::core::ProjectionType SkyContextController::projectionType() const noexcept
{
    return m_view.projectionType();
}

const SkyThemeRenderPalette& SkyContextController::renderTheme() const noexcept
{
    return m_themePalette->definition().render;
}

QString SkyContextController::utcTimeText() const
{
    return SkyQtTimeCodec::formatTimeText(SkyContextTimeCodec::toQDateTimeUtc(m_location.utcTime()));
}

QString SkyContextController::utcDateText() const
{
    return SkyContextUtcDateTimeTextCodec::formatDate(SkyContextTimeCodec::toQDateTimeUtc(m_location.utcTime()));
}

QString SkyContextController::latitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_location.observer().latitudeDeg);
}

QString SkyContextController::longitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_location.observer().longitudeDeg);
}

QString SkyContextController::elevationText() const
{
    return SkyContextTextFormatter::formatElevation(m_location.observer().elevationMeters);
}

QString SkyContextController::locationSourceText() const
{
    return SkyContextLocationSourceCodec::toString(m_location.source());
}

QStringList SkyContextController::locationSourceOptions() const
{
    return SkyContextLocationSourceCodec::availableOptions();
}

QString SkyContextController::selectedCityId() const
{
    return m_location.selectedCityId();
}

QString SkyContextController::selectedCityDisplayText() const
{
    return m_location.selectedCityDisplayText();
}

QAbstractItemModel* SkyContextController::cityCatalogModel() const noexcept
{
    return m_locationCatalogModel.get();
}

QStringList SkyContextController::projectionTypeOptions() const
{
    return SkyContextProjectionTypeCodec::availableOptions();
}

QString SkyContextController::projectionTypeText() const
{
    return SkyContextProjectionTypeCodec::toString(m_view.projectionType());
}

QString SkyContextController::themeId() const
{
    return m_themePalette != nullptr ? m_themePalette->id() : QString();
}

QVariantList SkyContextController::themeOptions() const
{
    return m_themeOptions;
}

QObject* SkyContextController::theme() const noexcept
{
    return m_themePalette.get();
}

SkyTimeController* SkyContextController::timeController() const noexcept
{
    return m_timeController.get();
}

QObject* SkyContextController::overlayLayers() const noexcept
{
    return m_overlayLayerSettings.get();
}

bool SkyContextController::logToTerminal() const noexcept
{
    return m_logToTerminal;
}

bool SkyContextController::logToFile() const noexcept
{
    return m_logToFile;
}

QString SkyContextController::logFilePath() const
{
    return m_logFilePath;
}

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

const SkyOverlayLayerVisibility& SkyContextController::overlayLayerVisibility() const noexcept
{
    static const SkyOverlayLayerVisibility kDefaultVisibility;
    return m_overlayLayerSettings != nullptr ? m_overlayLayerSettings->visibility() : kDefaultVisibility;
}

QString SkyContextController::locationStatusText() const
{
    return m_location.statusText();
}

void SkyContextController::setLocationStatusText(const QString& locationStatusText)
{
    if (m_location.setStatusText(locationStatusText)) {
        emit locationStatusTextChanged();
    }
}

void SkyContextController::updateLocationStatusText()
{
    switch (m_location.source()) {
    case SkyContextLocationSource::CurrentDevice:
        setLocationStatusText("Location: Current device");
        return;
    case SkyContextLocationSource::City:
        if (!m_location.selectedCityDisplayText().isEmpty()) {
            setLocationStatusText(QString("Location: City - %1").arg(m_location.selectedCityDisplayText()));
            return;
        }

        setLocationStatusText("Location: City");
        return;
    case SkyContextLocationSource::Custom:
        setLocationStatusText("Location: Custom coordinates");
        return;
    }
}

void SkyContextController::applyCurrentLocation(const QGeoCoordinate& coordinate)
{
#if SKYGATE_HAS_POSITIONING
    if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
        return;
    }

    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.latitudeDeg = coordinate.latitude();
    nextObserver.longitudeDeg = coordinate.longitude();
    if (std::isfinite(coordinate.altitude())) {
        nextObserver.elevationMeters = coordinate.altitude();
    }

    if (!nextObserver.isValid()) {
        return;
    }

    applyObserverLocation(nextObserver);
    setLocationStatusText("Location: Using current location");
#else
    static_cast<void>(coordinate);
#endif
}

void SkyContextController::setSelectedSearchTarget(const QString& targetKind, const QString& targetId)
{
    if (m_search.setSelectedTarget(targetKind, targetId)) {
        emit selectedSearchTargetChanged();
    }
}

void SkyContextController::clearSelectedSearchTarget()
{
    setSelectedSearchTarget(QString(), QString());
}

void SkyContextController::setTrackedTarget(
    const QString& targetKind, const QString& targetId, const QString& displayText
)
{
    if (m_search.setTrackedTarget(targetKind, targetId, displayText)) {
        emit trackedTargetChanged();
    }
}

QString SkyContextController::catalogStatusText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->statusText() : QString();
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

QString SkyContextController::catalogDatasetInfoText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->datasetInfoText() : QString();
}

QString SkyContextController::catalogParticipationSummary() const
{
    return m_catalogManager != nullptr ? m_catalogManager->participationSummary() : QString();
}

QString SkyContextController::deepSkyCatalogInfoText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->deepSkyCatalogInfoText() : QString();
}

QAbstractItemModel* SkyContextController::objectSearchModel() const noexcept
{
    return m_objectSearchModel.get();
}

QAbstractItemModel* SkyContextController::catalogSourcePresetModel() const noexcept
{
    return m_catalogSourcePresetModel.get();
}

QAbstractItemModel* SkyContextController::catalogSourceCollectionModel() const noexcept
{
    return m_catalogSourceCollectionModel.get();
}

bool SkyContextController::downloadingCatalog() const noexcept
{
    return m_catalogManager != nullptr && m_catalogManager->downloadingCatalog();
}

bool SkyContextController::catalogProcessing() const noexcept
{
    return m_catalogManager != nullptr && m_catalogManager->catalogProcessing();
}

const skygate::core::ObservationContext& SkyContextController::skyContext() const noexcept
{
    return m_location.context();
}

std::uint64_t SkyContextController::catalogRevision() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogRevision() : 0U;
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

std::span<const skygate::ephemeris::BaseCelestialBody* const> SkyContextController::catalogBodies() const noexcept
{
    const auto* starCatalog = m_catalogManager != nullptr ? m_catalogManager->starCatalog() : nullptr;
    return starCatalog != nullptr ? starCatalog->bodies()
                                  : std::span<const skygate::ephemeris::BaseCelestialBody* const>{};
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

QHash<QString, QString> SkyContextController::catalogSourceTitles() const
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceTitles() : QHash<QString, QString>{};
}

std::span<const QString> SkyContextController::catalogSourceIds() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceIds() : std::span<const QString>{};
}

const std::vector<QStringList>& SkyContextController::catalogSourceContributorIds() const noexcept
{
    static const std::vector<QStringList> kEmptyContributors;
    return m_catalogManager != nullptr ? m_catalogManager->contributorSourceIds() : kEmptyContributors;
}

std::span<const SkyContextController::ConstellationLineRef> SkyContextController::constellationLineRefs() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->constellationLineRefs()
                                       : std::span<const ConstellationLineRef>{};
}

std::span<const SkyContextController::ConstellationAnchorGroup>
SkyContextController::constellationAnchorGroups() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->constellationAnchorGroups()
                                       : std::span<const ConstellationAnchorGroup>{};
}

std::span<const SkyContextController::ConstellationLineRef> SkyContextController::resolvedConstellationLineRefs() const
{
    return m_catalogManager != nullptr ? m_catalogManager->resolvedConstellationLineRefs()
                                       : std::span<const ConstellationLineRef>{};
}

std::span<const SkyContextController::ConstellationAnchorGroup>
SkyContextController::resolvedConstellationAnchorGroups() const
{
    return m_catalogManager != nullptr ? m_catalogManager->resolvedConstellationAnchorGroups()
                                       : std::span<const ConstellationAnchorGroup>{};
}

int SkyContextController::catalogPresetIndex() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogPresetIndex() : 0;
}

int SkyContextController::deepSkyCatalogPresetIndex() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->deepSkyCatalogPresetIndex() : 0;
}

void SkyContextController::refreshObjectSearchModel()
{
    if (m_objectSearchModel == nullptr) {
        return;
    }

    m_objectSearchModel->setCatalogData(catalogBodies(), resolvedConstellationAnchorGroups());
}

void SkyContextController::refreshCatalogSourceCollectionModel()
{
    if (m_catalogSourceCollectionModel == nullptr || m_catalogManager == nullptr) {
        return;
    }

    QVector<SkyCatalogSourceCollectionModel::SourceEntry> entries;
    const QVector<SkyCatalogManager::SourceViewEntry> sourceView = m_catalogManager->sourceViewEntries();
    entries.reserve(sourceView.size());
    for (const SkyCatalogManager::SourceViewEntry& viewEntry : sourceView) {
        SkyCatalogSourceCollectionModel::SourceEntry entry;
        entry.instanceId = viewEntry.instanceId;
        entry.title = viewEntry.title;
        entry.version = viewEntry.version;
        entry.category = SkyCatalogPresets::categoryForPolicy(viewEntry.policy);
        entry.enabled = viewEntry.enabled;
        entry.bundled = viewEntry.bundled;
        entry.busy = viewEntry.busy;
        entry.hasError = viewEntry.hasError;
        entry.statusText = viewEntry.statusText;
        entry.objectCount = static_cast<int>(viewEntry.objectCount);
        entries.push_back(std::move(entry));
    }
    m_catalogSourceCollectionModel->replaceEntries(std::move(entries));
}

void SkyContextController::setCatalogPresetIndex(const int catalogPresetIndex)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setCatalogPresetIndex(catalogPresetIndex);
    }
}

void SkyContextController::setDeepSkyCatalogPresetIndex(const int deepSkyCatalogPresetIndex)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setDeepSkyCatalogPresetIndex(deepSkyCatalogPresetIndex);
    }
}

void SkyContextController::setThemeId(const QString& themeId)
{
    if (m_themePalette == nullptr || m_themeRepository == nullptr) {
        return;
    }

    const SkyThemeDefinition& resolvedTheme = m_themeRepository->themeById(themeId);
    if (m_themePalette->id() == resolvedTheme.id) {
        return;
    }

    m_themePalette->setDefinition(resolvedTheme);
    emit themeChanged();
    emit skyContextChanged();
}

void SkyContextController::applyLoggingConfiguration()
{
    skygate::ui::SkyLoggingConfiguration configuration = skygate::ui::SkyLogging::configuration();
    configuration.logToTerminal = m_logToTerminal;
    configuration.logToFile = m_logToFile;
    configuration.logFilePath =
        m_logFilePath.trimmed().isEmpty() ? skygate::ui::SkyLogging::defaultLogFilePath() : m_logFilePath.trimmed();
    skygate::ui::SkyLogging::configure(configuration);
}

void SkyContextController::setLogToTerminal(const bool logToTerminal)
{
    if (m_logToTerminal == logToTerminal) {
        return;
    }

    m_logToTerminal = logToTerminal;
    applyLoggingConfiguration();
    emit loggingChanged();
}

void SkyContextController::setLogToFile(const bool logToFile)
{
    if (m_logToFile == logToFile) {
        return;
    }

    m_logToFile = logToFile;
    applyLoggingConfiguration();
    emit loggingChanged();
}

void SkyContextController::setLogFilePath(const QString& logFilePath)
{
    const QString normalizedPath =
        logFilePath.trimmed().isEmpty() ? skygate::ui::SkyLogging::defaultLogFilePath() : logFilePath.trimmed();
    if (m_logFilePath == normalizedPath) {
        return;
    }

    m_logFilePath = normalizedPath;
    applyLoggingConfiguration();
    emit loggingChanged();
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

QString SkyContextController::catalogUrlText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogUrlText() : QString();
}

void SkyContextController::setCatalogUrlText(const QString& catalogUrlText)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setCatalogUrlText(catalogUrlText);
    }
}

QString SkyContextController::deepSkyCatalogUrlText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->deepSkyCatalogUrlText() : QString();
}

void SkyContextController::setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setDeepSkyCatalogUrlText(deepSkyCatalogUrlText);
    }
}

QString SkyContextController::catalogSourcePresetId(const int index) const
{
    return m_catalogSourcePresetModel != nullptr ? m_catalogSourcePresetModel->sourceIdAt(index) : QString();
}

QString SkyContextController::catalogSourcePresetCategory(const int index) const
{
    return m_catalogSourcePresetModel != nullptr ? m_catalogSourcePresetModel->categoryAt(index) : QString();
}

bool SkyContextController::catalogSourcePresetIsCustom(const int index) const
{
    return m_catalogSourcePresetModel != nullptr && m_catalogSourcePresetModel->isCustomAt(index);
}

void SkyContextController::clearSelectedCity()
{
    const SkySelectedCityChange change = m_location.clearSelectedCity();
    if (!change.idChanged && !change.displayTextChanged) {
        return;
    }

    if (change.idChanged) {
        emit selectedCityIdChanged();
    }
    if (change.displayTextChanged) {
        emit selectedCityDisplayTextChanged();
    }
}

void SkyContextController::setLocationSource(const SkyContextLocationSource locationSource)
{
    SkyContextLocationSource nextLocationSource = locationSource;
    if (!SkyContextLocationSourceCodec::isAvailable(nextLocationSource)) {
        nextLocationSource = SkyContextLocationSource::Custom;
    }

    if (m_location.source() == nextLocationSource) {
        if (m_location.source() != SkyContextLocationSource::City) {
            clearSelectedCity();
        }
        updateLocationStatusText();
        return;
    }

    static_cast<void>(m_location.setSource(nextLocationSource));
    if (m_location.source() != SkyContextLocationSource::City) {
        clearSelectedCity();
    }

    emit locationSourceTextChanged();
    updateLocationStatusText();
}

void SkyContextController::setLocationSourceText(const QString& locationSourceText)
{
    const auto parsedLocationSource = SkyContextLocationSourceCodec::fromString(locationSourceText);
    const SkyContextLocationSource nextLocationSource = parsedLocationSource.has_value()
                                                            ? parsedLocationSource.value()
                                                            : SkyContextLocationSourceCodec::defaultSource();
    setLocationSource(nextLocationSource);
}

bool SkyContextController::applySelectedCityId(const QString& cityId)
{
    if (m_locationCatalogModel == nullptr) {
        return false;
    }

    const auto cityEntry = m_locationCatalogModel->entryForCityId(cityId);
    if (!cityEntry.has_value()) {
        return false;
    }

    const QString displayText = cityEntry->displayText();
    const SkySelectedCityChange cityChange = m_location.setSelectedCity(cityEntry->id, displayText);

    if (cityChange.idChanged) {
        emit selectedCityIdChanged();
    }
    if (cityChange.displayTextChanged) {
        emit selectedCityDisplayTextChanged();
    }
    emit locationSourceTextChanged();

    skygate::core::GeoLocation observer = m_location.observer();
    observer.latitudeDeg = cityEntry->latitudeDeg;
    observer.longitudeDeg = cityEntry->longitudeDeg;
    applyObserverLocation(observer);
    updateLocationStatusText();
    return true;
}

void SkyContextController::setSelectedCityId(const QString& selectedCityId)
{
    if (selectedCityId.trimmed().isEmpty()) {
        clearSelectedCity();
        if (m_location.source() == SkyContextLocationSource::City) {
            updateLocationStatusText();
        }
        return;
    }

    (void)applySelectedCityId(selectedCityId.trimmed());
}
