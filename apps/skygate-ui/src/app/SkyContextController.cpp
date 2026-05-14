#include "SkyContextController.hpp"

#include "LocationCatalogModel.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyLogging.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySettingsStore.hpp"
#include "SkyTimeController.hpp"

#include <QDateTime>

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

using namespace skygate::ui::internal;

namespace {

[[nodiscard]] std::optional<skygate::ephemeris::AstronomicalEpoch>
astronomicalEpochFromUtcDateTime(const QDateTime& utcDateTime) noexcept
{
    const QDateTime normalizedUtcDateTime = utcDateTime.toUTC();
    const QDate date = normalizedUtcDateTime.date();
    const QTime time = normalizedUtcDateTime.time();
    const auto astronomicalYear = skygate::ephemeris::astronomicalYearFromHistoricalYear(date.year());
    if (!astronomicalYear.has_value()) {
        return std::nullopt;
    }

    return skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = *astronomicalYear,
        .month = date.month(),
        .day = date.day(),
        .hour = time.hour(),
        .minute = time.minute(),
        .second = time.second(),
        .nanosecond = static_cast<std::uint32_t>(time.msec()) * 1'000'000U,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
}

void appendRevisionComponent(std::uint64_t& revision, const std::string_view value) noexcept
{
    constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;
    for (const char character : value) {
        revision ^= static_cast<unsigned char>(character);
        revision *= kFnvPrime;
    }
    revision ^= 0xffU;
    revision *= kFnvPrime;
}

[[nodiscard]] std::uint64_t textDataAssetRevision(
    const std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot>& snapshot,
    const std::uint64_t dataRevision,
    const bool earthOrientation
) noexcept
{
    if (snapshot == nullptr) {
        return 0U;
    }

    const std::optional<skygate::ephemeris::EphemerisTextDataAsset> asset =
        earthOrientation ? snapshot->earthOrientationDataAsset() : snapshot->leapSecondTableAsset();
    if (!asset.has_value()) {
        return dataRevision;
    }

    constexpr std::uint64_t kFnvOffsetBasis = 14'695'981'039'346'656'037ULL;
    std::uint64_t revision = kFnvOffsetBasis;
    revision ^= dataRevision;
    revision *= 1'099'511'628'211ULL;
    appendRevisionComponent(revision, asset->id);
    appendRevisionComponent(revision, asset->version);
    appendRevisionComponent(revision, asset->provenance);
    return revision;
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
      m_locationCatalogModel(std::make_unique<LocationCatalogModel>(this)),
      m_themePalette(std::make_unique<SkyThemePalette>(this)),
      m_themeRepository(std::make_unique<SkyThemeRepository>()),
      m_timeController(std::make_unique<SkyTimeController>(*m_timeSource, this)),
      m_overlayLayerSettings(std::make_unique<SkyOverlayLayerSettings>(this)),
      m_settingsStore(std::make_unique<SkySettingsStore>()),
      m_ephemerisDataManager(std::make_unique<SkyEphemerisDataManager>(m_settingsStore.get(), this)),
      m_ephemerisEngine(std::move(ephemerisEngine)),
      m_ephemerisDataSetManifest(initializationOptions.ephemerisFactoryInputs.dataSetManifest),
      m_ephemerisDataManifest(initializationOptions.ephemerisFactoryInputs.dataManifest),
      m_ephemerisTimeScaleService(std::move(initializationOptions.ephemerisFactoryInputs.timeScaleService)),
      m_ephemerisEarthOrientationProvider(
          std::move(initializationOptions.ephemerisFactoryInputs.earthOrientationProvider)
      ),
      m_ephemerisCalcephKernelRuntime(std::move(initializationOptions.ephemerisFactoryInputs.calcephKernelRuntime)),
      m_ephemerisDiagnosticsSink(initializationOptions.ephemerisFactoryInputs.diagnosticsSink),
      m_catalogManager(std::make_unique<SkyCatalogManager>(m_settingsStore.get(), std::move(starCatalog), this)),
      m_objectSearchModel(std::make_unique<SkyObjectSearchModel>(this))
{
    m_logFilePath = skygate::ui::SkyLogging::defaultLogFilePath();
    m_location.setPositionSource(initializationOptions.positionSource);
    m_location.setRequestLocationPermission(initializationOptions.requestLocationPermission);
    if (m_ephemerisEngine != nullptr) {
        m_ephemerisEngineKind = m_ephemerisEngine->kind();
        m_ephemerisEngineOptions = m_ephemerisEngine->options();
    }
    m_ephemerisUserSettings.engineKind = m_ephemerisEngineKind;
    m_ephemerisUserSettings.correctionFlags = m_ephemerisEngineOptions.correctionFlags;
    m_ephemerisUserSettings.fallbackToSimpleEngine = m_ephemerisEngineOptions.fallbackToSimpleEngine;
    m_ephemerisUserSettings.refractionEnabled = m_ephemerisEngineOptions.enableAtmosphericRefraction;
    m_ephemerisUserSettings.atmosphericPressureHpa = m_ephemerisEngineOptions.atmosphericPressureHpa;
    m_ephemerisUserSettings.atmosphericTemperatureC = m_ephemerisEngineOptions.atmosphericTemperatureC;
    m_ephemerisUserSettings.relativeHumidity = m_ephemerisEngineOptions.relativeHumidity;
    m_ephemerisUserSettings.observingWavelengthMicrometers = m_ephemerisEngineOptions.observingWavelengthMicrometers;
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
        emit ephemerisDataChanged();
        emit skyContextChanged();
    });

    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::statusTextChanged,
        this,
        &SkyContextController::catalogStatusTextChanged
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
    refreshObjectSearchModel();

    m_location.setUtcTime(m_timeSource->nowUtc());
    m_timeController->setUtcTimePoint(m_location.utcTime());
    if (initializationOptions.loadSettings) {
        loadSettings();
    }
    updateLocationStatusText();

    m_timer.setInterval(SkyContextControllerConstants::kTickIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SkyContextController::tickUtcTime);
    m_timer.start();

    if (initializationOptions.initializeLocation && m_location.source() == SkyContextLocationSource::CurrentDevice) {
        initializeCurrentLocation();
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
    return SkyContextTimeCodec::toQDateTimeUtc(m_location.utcTime()).toString("HH:mm:ss");
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
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->statusText() : QString();
}

QString SkyContextController::catalogDatasetInfoText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->datasetInfoText() : QString();
}

QString SkyContextController::deepSkyCatalogInfoText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->deepSkyCatalogInfoText() : QString();
}

QAbstractItemModel* SkyContextController::objectSearchModel() const noexcept
{
    return m_objectSearchModel.get();
}

bool SkyContextController::downloadingCatalog() const noexcept
{
    return m_catalogManager != nullptr && m_catalogManager->downloadingCatalog();
}

bool SkyContextController::catalogProcessing() const noexcept
{
    return m_catalogManager != nullptr && m_catalogManager->catalogProcessing();
}

const skygate::core::SkyContext& SkyContextController::skyContext() const noexcept
{
    return m_location.context();
}

std::uint64_t SkyContextController::catalogRevision() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogRevision() : 0U;
}

const skygate::ephemeris::IEphemerisEngine* SkyContextController::ephemerisEngine() const noexcept
{
    return m_ephemerisEngine.get();
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
SkyContextController::ephemerisRequestContextFor(const skygate::core::SkyContext& skyContext) const
{
    EphemerisRequestContext context;
    context.request.context = skyContext;
    context.request.options = m_ephemerisEngineOptions;
    context.request.options.engineKind = m_ephemerisEngineKind;
    context.activeDataSnapshot = activeEphemerisDataSnapshot();
    context.engineOptionsRevision = m_ephemerisOptionsRevision;
    context.ephemerisDataRevision = ephemerisDataRevision();
    context.earthOrientationDataRevision =
        textDataAssetRevision(context.activeDataSnapshot, context.ephemerisDataRevision, true);
    context.leapSecondDataRevision =
        textDataAssetRevision(context.activeDataSnapshot, context.ephemerisDataRevision, false);
    context.catalogRevision = catalogRevision();

    if (const auto epoch = astronomicalEpochFromUtcDateTime(SkyContextTimeCodec::toQDateTimeUtc(skyContext.utcTime));
        epoch.has_value()) {
        context.request.epoch = *epoch;
    }

    return context;
}

std::span<const skygate::ephemeris::CelestialBody> SkyContextController::catalogBodies() const noexcept
{
    const auto* starCatalog = m_catalogManager != nullptr ? m_catalogManager->starCatalog() : nullptr;
    return starCatalog != nullptr ? starCatalog->bodies() : std::span<const skygate::ephemeris::CelestialBody>{};
}

void SkyContextController::rebuildEphemerisEngine()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = m_ephemerisEngineKind;
    request.catalogBodies = catalogBodies();
    request.options = m_ephemerisEngineOptions;
    request.options.engineKind = m_ephemerisEngineKind;
    request.dataSetManifest = m_ephemerisDataSetManifest;
    request.dataManifest = m_ephemerisDataManifest;
    request.activeDataSnapshot = activeEphemerisDataSnapshot();
    request.timeScaleService = m_ephemerisTimeScaleService;
    request.earthOrientationProvider = m_ephemerisEarthOrientationProvider;
    request.calcephKernelRuntime = m_ephemerisCalcephKernelRuntime;
    request.diagnosticsSink = m_ephemerisDiagnosticsSink;
    request.fallbackPolicy = m_ephemerisEngineKind == skygate::ephemeris::EphemerisEngineKind::HighPrecision
                                     && !m_ephemerisEngineOptions.fallbackToSimpleEngine
                                 ? skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision
                                 : skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    auto result = skygate::ephemeris::createEphemerisEngine(request);
    if (result.usedSimpleEngineFallback()
        && m_ephemerisEngineKind == skygate::ephemeris::EphemerisEngineKind::HighPrecision
        && m_ephemerisEngine != nullptr
        && m_ephemerisEngine->kind() == skygate::ephemeris::EphemerisEngineKind::HighPrecision) {
        return;
    }
    if (result.engine != nullptr) {
        m_ephemerisEngine = std::move(result.engine);
    }
}

QStringList SkyContextController::catalogSourceLabels() const
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceLabels() : QStringList{};
}

std::span<const std::uint8_t> SkyContextController::catalogSourceIds() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceIds() : std::span<const std::uint8_t>{};
}

std::span<const SkyContextController::ConstellationLineRef> SkyContextController::constellationLineRefs() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->constellationLineRefs()
                                       : std::span<const ConstellationLineRef>{};
}

std::span<const SkyContextController::ConstellationLabelRef>
SkyContextController::constellationLabelRefs() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->constellationLabelRefs()
                                       : std::span<const ConstellationLabelRef>{};
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

    m_objectSearchModel->setCatalogData(catalogBodies(), constellationLabelRefs());
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
