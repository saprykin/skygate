#include "SkyContextController.hpp"

#include "LocationCatalogModel.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySettingsStore.hpp"

#include <QDateTime>

#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/core/SystemTimeSource.hpp"

#include <memory>
#include <utility>

using namespace skygate::ui::internal;

SkyContextController::SkyContextController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    QObject* parent
)
    : SkyContextController(
          std::move(starCatalog),
          std::move(ephemerisEngine),
          InitializationOptions {
              .loadSettings = true,
              .initializeLocation = true
          },
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
    : QObject(parent)
    , m_locationCatalogModel(std::make_unique<LocationCatalogModel>(this))
    , m_themePalette(std::make_unique<SkyThemePalette>(this))
    , m_themeRepository(std::make_unique<SkyThemeRepository>())
    , m_overlayLayerSettings(std::make_unique<SkyOverlayLayerSettings>(this))
    , m_settingsStore(std::make_unique<SkySettingsStore>())
    , m_catalogManager(std::make_unique<SkyCatalogManager>(
          m_settingsStore.get(),
          std::move(starCatalog),
          std::move(ephemerisEngine),
          this
      ))
    , m_objectSearchModel(std::make_unique<SkyObjectSearchModel>(this))
{
    m_themeOptions = m_themeRepository->themeOptions();
    m_themePalette->setDefinition(m_themeRepository->defaultTheme());
    connect(
        m_overlayLayerSettings.get(),
        &SkyOverlayLayerSettings::visibilityChanged,
        this,
        &SkyContextController::skyContextChanged
    );

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
    connect(
        m_catalogManager.get(),
        &SkyCatalogManager::catalogChanged,
        this,
        [this] {
            refreshObjectSearchModel();
            if (!recenterTrackedTarget(true)) {
                emit skyContextChanged();
            }
        }
    );
    refreshObjectSearchModel();

    m_view.projection = skygate::core::createProjection(m_view.projectionType);
    const skygate::core::SystemTimeSource systemTimeSource;
    m_location.context.utcTime = systemTimeSource.nowUtc();
    if (initializationOptions.loadSettings) {
        loadSettings();
    }
    updateLocationStatusText();

    m_timer.setInterval(SkyContextControllerConstants::kTickIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SkyContextController::tickUtcTime);
    m_timer.start();

    if (
        initializationOptions.initializeLocation
        && m_location.source == SkyContextLocationSource::CurrentDevice
    ) {
        initializeCurrentLocation();
    }
}

SkyContextController::~SkyContextController() = default;

bool SkyContextController::live() const noexcept
{
    return m_timeline.live;
}

QString SkyContextController::selectedSearchTargetKind() const
{
    return m_search.selectedTargetKind;
}

QString SkyContextController::selectedSearchTargetId() const
{
    return m_search.selectedTargetId;
}

bool SkyContextController::hasTrackedTarget() const
{
    return !m_search.trackedTargetKind.trimmed().isEmpty()
        && !m_search.trackedTargetId.trimmed().isEmpty();
}

QString SkyContextController::trackedTargetKind() const
{
    return m_search.trackedTargetKind;
}

QString SkyContextController::trackedTargetId() const
{
    return m_search.trackedTargetId;
}

QString SkyContextController::trackedTargetDisplayText() const
{
    return m_search.trackedTargetDisplayText;
}

double SkyContextController::speedMultiplier() const noexcept
{
    return m_timeline.speedMultiplier;
}

int SkyContextController::stepSeconds() const noexcept
{
    return m_timeline.stepSeconds;
}

double SkyContextController::magnitudeCutoff() const noexcept
{
    return m_view.magnitudeCutoff;
}

double SkyContextController::viewCenterAltitudeDeg() const noexcept
{
    return m_view.centerAltitudeDeg;
}

double SkyContextController::viewCenterAzimuthDeg() const noexcept
{
    return m_view.centerAzimuthDeg;
}

double SkyContextController::viewFieldOfViewDeg() const noexcept
{
    return m_view.fieldOfViewDeg;
}

skygate::core::ProjectionType SkyContextController::projectionType() const noexcept
{
    return m_view.projectionType;
}

const SkyThemeRenderPalette& SkyContextController::renderTheme() const noexcept
{
    return m_themePalette->definition().render;
}

QString SkyContextController::utcTimeText() const
{
    return SkyContextTimeCodec::toQDateTimeUtc(m_location.context.utcTime).toString("HH:mm:ss");
}

QString SkyContextController::utcDateText() const
{
    return SkyContextUtcDateTimeTextCodec::formatDate(
        SkyContextTimeCodec::toQDateTimeUtc(m_location.context.utcTime)
    );
}

QString SkyContextController::latitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_location.context.observer.latitudeDeg);
}

QString SkyContextController::longitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_location.context.observer.longitudeDeg);
}

QString SkyContextController::elevationText() const
{
    return SkyContextTextFormatter::formatElevation(m_location.context.observer.elevationMeters);
}

QString SkyContextController::locationSourceText() const
{
    return SkyContextLocationSourceCodec::toString(m_location.source);
}

QStringList SkyContextController::locationSourceOptions() const
{
    return SkyContextLocationSourceCodec::availableOptions();
}

QString SkyContextController::selectedCityId() const
{
    return m_location.selectedCityId;
}

QString SkyContextController::selectedCityDisplayText() const
{
    return m_location.selectedCityDisplayText;
}

QAbstractItemModel* SkyContextController::cityCatalogModel() const noexcept
{
    return m_locationCatalogModel.get();
}

QString SkyContextController::projectionTypeText() const
{
    return SkyContextProjectionTypeCodec::toString(m_view.projectionType);
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

QObject* SkyContextController::overlayLayers() const noexcept
{
    return m_overlayLayerSettings.get();
}

const SkyOverlayLayerVisibility& SkyContextController::overlayLayerVisibility() const noexcept
{
    static const SkyOverlayLayerVisibility kDefaultVisibility;
    return m_overlayLayerSettings != nullptr
        ? m_overlayLayerSettings->visibility()
        : kDefaultVisibility;
}

QString SkyContextController::locationStatusText() const
{
    return m_location.statusText;
}

void SkyContextController::setLocationStatusText(const QString& locationStatusText)
{
    if (m_location.statusText == locationStatusText) {
        return;
    }

    m_location.statusText = locationStatusText;
    emit locationStatusTextChanged();
}

void SkyContextController::updateLocationStatusText()
{
    switch (m_location.source) {
    case SkyContextLocationSource::CurrentDevice:
        setLocationStatusText("Location: Current device");
        return;
    case SkyContextLocationSource::City:
        if (!m_location.selectedCityDisplayText.isEmpty()) {
            setLocationStatusText(
                QString("Location: City - %1").arg(m_location.selectedCityDisplayText)
            );
            return;
        }

        setLocationStatusText("Location: City");
        return;
    case SkyContextLocationSource::Custom:
        setLocationStatusText("Location: Custom coordinates");
        return;
    }
}

void SkyContextController::setSelectedSearchTarget(
    const QString& targetKind,
    const QString& targetId
)
{
    const QString normalizedTargetKind = targetKind.trimmed();
    const QString normalizedTargetId = targetId.trimmed();
    if (
        m_search.selectedTargetKind == normalizedTargetKind
        && m_search.selectedTargetId == normalizedTargetId
    ) {
        return;
    }

    m_search.selectedTargetKind = normalizedTargetKind;
    m_search.selectedTargetId = normalizedTargetId;
    emit selectedSearchTargetChanged();
}

void SkyContextController::clearSelectedSearchTarget()
{
    setSelectedSearchTarget(QString(), QString());
}

void SkyContextController::setTrackedTarget(
    const QString& targetKind,
    const QString& targetId,
    const QString& displayText
)
{
    const QString normalizedTargetKind = targetKind.trimmed();
    const QString normalizedTargetId = targetId.trimmed();
    const QString normalizedDisplayText = displayText.trimmed();
    if (
        m_search.trackedTargetKind == normalizedTargetKind
        && m_search.trackedTargetId == normalizedTargetId
        && m_search.trackedTargetDisplayText == normalizedDisplayText
    ) {
        return;
    }

    m_search.trackedTargetKind = normalizedTargetKind;
    m_search.trackedTargetId = normalizedTargetId;
    m_search.trackedTargetDisplayText = normalizedDisplayText;
    emit trackedTargetChanged();
}

QString SkyContextController::catalogStatusText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->statusText() : QString();
}

QString SkyContextController::catalogDatasetInfoText() const
{
    return m_catalogManager != nullptr ? m_catalogManager->datasetInfoText() : QString();
}

QString SkyContextController::deepSkyCatalogInfoText() const
{
    return m_catalogManager != nullptr
        ? m_catalogManager->deepSkyCatalogInfoText()
        : QString();
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
    return m_location.context;
}

std::uint64_t SkyContextController::catalogRevision() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogRevision() : 0U;
}

const skygate::ephemeris::IEphemerisEngine* SkyContextController::ephemerisEngine() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->ephemerisEngine() : nullptr;
}

std::span<const skygate::ephemeris::CelestialBody>
SkyContextController::catalogBodies() const noexcept
{
    const auto* starCatalog = m_catalogManager != nullptr
        ? m_catalogManager->starCatalog()
        : nullptr;
    return starCatalog != nullptr
        ? starCatalog->bodies()
        : std::span<const skygate::ephemeris::CelestialBody> {};
}

QStringList SkyContextController::catalogSourceLabels() const
{
    return m_catalogManager != nullptr ? m_catalogManager->sourceLabels() : QStringList {};
}

std::span<const std::uint8_t> SkyContextController::catalogSourceIds() const noexcept
{
    return m_catalogManager != nullptr
        ? m_catalogManager->sourceIds()
        : std::span<const std::uint8_t> {};
}

std::span<const SkyContextController::ConstellationLineRef>
SkyContextController::constellationLineRefs() const noexcept
{
    return m_catalogManager != nullptr
        ? m_catalogManager->constellationLineRefs()
        : std::span<const ConstellationLineRef> {};
}

std::span<const SkyContextController::ConstellationLabelRef>
SkyContextController::constellationLabelRefs() const noexcept
{
    return m_catalogManager != nullptr
        ? m_catalogManager->constellationLabelRefs()
        : std::span<const ConstellationLabelRef> {};
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
    const bool hadSelectedCityId = !m_location.selectedCityId.isEmpty();
    const bool hadSelectedCityDisplayText = !m_location.selectedCityDisplayText.isEmpty();
    if (!hadSelectedCityId && !hadSelectedCityDisplayText) {
        return;
    }

    m_location.selectedCityId.clear();
    m_location.selectedCityDisplayText.clear();

    if (hadSelectedCityId) {
        emit selectedCityIdChanged();
    }
    if (hadSelectedCityDisplayText) {
        emit selectedCityDisplayTextChanged();
    }
}

void SkyContextController::setLocationSource(const SkyContextLocationSource locationSource)
{
    SkyContextLocationSource nextLocationSource = locationSource;
    if (!SkyContextLocationSourceCodec::isAvailable(nextLocationSource)) {
        nextLocationSource = SkyContextLocationSource::Custom;
    }

    if (m_location.source == nextLocationSource) {
        if (m_location.source != SkyContextLocationSource::City) {
            clearSelectedCity();
        }
        updateLocationStatusText();
        return;
    }

    m_location.source = nextLocationSource;
    if (m_location.source != SkyContextLocationSource::City) {
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

    const bool cityIdChanged = m_location.selectedCityId != cityEntry->id;
    const QString displayText = cityEntry->displayText();
    const bool displayTextChanged = m_location.selectedCityDisplayText != displayText;

    m_location.selectedCityId = cityEntry->id;
    m_location.selectedCityDisplayText = displayText;
    m_location.source = SkyContextLocationSource::City;

    if (cityIdChanged) {
        emit selectedCityIdChanged();
    }
    if (displayTextChanged) {
        emit selectedCityDisplayTextChanged();
    }
    emit locationSourceTextChanged();

    skygate::core::GeoLocation observer = m_location.context.observer;
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
        if (m_location.source == SkyContextLocationSource::City) {
            updateLocationStatusText();
        }
        return;
    }

    (void)applySelectedCityId(selectedCityId.trimmed());
}
