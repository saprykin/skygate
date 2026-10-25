#include "SkyContextController.hpp"
#include "SkyContextLocationSource.hpp"
#include "SkyContextLocationSourceCodec.hpp"
#include "SkyContextProjectionTypeCodec.hpp"
#include "SkyContextTextFormatter.hpp"
#include "SkySelectedCityChange.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "catalog/SkyCatalogPresets.hpp"
#include "catalog/SkyCatalogSourceCollectionModel.hpp"
#include "catalog/SkyCatalogSourcePresetModel.hpp"
#include "diagnostics/SkyLogging.hpp"
#include "diagnostics/SkyLoggingConfiguration.hpp"
#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "ephemeris/SkyEphemerisSettingsPolicy.hpp"
#include "ephemeris/SkyEphemerisUpdateWorkflow.hpp"
#include "location/LocationCatalogModel.hpp"
#include "location/SkyDeviceLocationAcquisition.hpp"
#include "location/SkyLocationStatusText.hpp"
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

#include <cmath>
#include <memory>
#include <utility>

using namespace skygate::ui::internal;

namespace {

constexpr int kUtcTickIntervalMs = 1000;

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
    return SkyQtTimeCodec::formatTimeText(SkyQtTimeCodec::toQDateTimeUtc(m_location.utcTime()));
}

QString SkyContextController::utcDateText() const
{
    return SkyQtTimeCodec::formatDateText(SkyQtTimeCodec::toQDateTimeUtc(m_location.utcTime()).date());
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
        setLocationStatusText(SkyLocationStatusText::currentDevice());
        return;
    case SkyContextLocationSource::City:
        if (!m_location.selectedCityDisplayText().isEmpty()) {
            setLocationStatusText(SkyLocationStatusText::cityNamed(m_location.selectedCityDisplayText()));
            return;
        }

        setLocationStatusText(SkyLocationStatusText::city());
        return;
    case SkyContextLocationSource::Custom:
        setLocationStatusText(SkyLocationStatusText::customCoordinates());
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
    setLocationStatusText(SkyLocationStatusText::usingCurrentLocation());
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

std::span<const skygate::ephemeris::BaseCelestialBody* const> SkyContextController::catalogBodies() const noexcept
{
    const auto* starCatalog = m_catalogManager != nullptr ? m_catalogManager->starCatalog() : nullptr;
    return starCatalog != nullptr ? starCatalog->bodies()
                                  : std::span<const skygate::ephemeris::BaseCelestialBody* const>{};
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
