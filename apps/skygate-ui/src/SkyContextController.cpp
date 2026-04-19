#include "SkyContextController.hpp"

#include "LocationCatalogModel.hpp"
#include "SkyCatalogManager.hpp"
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
    , m_settingsStore(std::make_unique<SkySettingsStore>())
    , m_catalogManager(std::make_unique<SkyCatalogManager>(
          m_settingsStore.get(),
          std::move(starCatalog),
          std::move(ephemerisEngine),
          this
      ))
{
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
        &SkyContextController::skyContextChanged
    );

    m_projection = skygate::core::createProjection(m_projectionType);
    const skygate::core::SystemTimeSource systemTimeSource;
    m_skyContext.utcTime = systemTimeSource.nowUtc();
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
        && m_locationSource == SkyContextLocationSource::CurrentDevice
    ) {
        initializeCurrentLocation();
    }
}

SkyContextController::~SkyContextController() = default;

bool SkyContextController::live() const noexcept
{
    return m_live;
}

double SkyContextController::speedMultiplier() const noexcept
{
    return m_speedMultiplier;
}

int SkyContextController::stepSeconds() const noexcept
{
    return m_stepSeconds;
}

double SkyContextController::magnitudeCutoff() const noexcept
{
    return m_magnitudeCutoff;
}

double SkyContextController::viewCenterAltitudeDeg() const noexcept
{
    return m_viewCenterAltitudeDeg;
}

double SkyContextController::viewCenterAzimuthDeg() const noexcept
{
    return m_viewCenterAzimuthDeg;
}

double SkyContextController::viewFieldOfViewDeg() const noexcept
{
    return m_viewFieldOfViewDeg;
}

skygate::core::ProjectionType SkyContextController::projectionType() const noexcept
{
    return m_projectionType;
}

QString SkyContextController::utcTimeText() const
{
    return SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toString("HH:mm:ss");
}

QString SkyContextController::utcDateText() const
{
    return SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toString("yyyy-MM-dd");
}

QString SkyContextController::latitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_skyContext.observer.latitudeDeg);
}

QString SkyContextController::longitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_skyContext.observer.longitudeDeg);
}

QString SkyContextController::elevationText() const
{
    return SkyContextTextFormatter::formatElevation(m_skyContext.observer.elevationMeters);
}

QString SkyContextController::locationSourceText() const
{
    return SkyContextLocationSourceCodec::toString(m_locationSource);
}

QStringList SkyContextController::locationSourceOptions() const
{
    return SkyContextLocationSourceCodec::availableOptions();
}

QString SkyContextController::selectedCityId() const
{
    return m_selectedCityId;
}

QString SkyContextController::selectedCityDisplayText() const
{
    return m_selectedCityDisplayText;
}

QAbstractItemModel* SkyContextController::cityCatalogModel() const noexcept
{
    return m_locationCatalogModel.get();
}

QString SkyContextController::projectionTypeText() const
{
    return SkyContextProjectionTypeCodec::toString(m_projectionType);
}

QString SkyContextController::projectionSampleText() const
{
    if (m_projection == nullptr) {
        return "Projection unavailable";
    }

    skygate::core::ProjectionParams params;
    params.center = {.altitudeDeg = m_viewCenterAltitudeDeg, .azimuthDeg = m_viewCenterAzimuthDeg};
    params.fovDeg = m_viewFieldOfViewDeg;
    params.rollDeg = 0.0;
    params.viewportWidth = 1100.0;
    params.viewportHeight = 760.0;

    const auto projected = m_projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 30.0, .azimuthDeg = 180.0},
        params
    );

    return QString("Sample x=%1 y=%2 visible=%3").arg(
        QString::number(projected.x, 'f', 1),
        QString::number(projected.y, 'f', 1),
        projected.isVisible ? "true" : "false"
    );
}

QString SkyContextController::locationStatusText() const
{
    return m_locationStatusText;
}

void SkyContextController::setLocationStatusText(const QString& locationStatusText)
{
    if (m_locationStatusText == locationStatusText) {
        return;
    }

    m_locationStatusText = locationStatusText;
    emit locationStatusTextChanged();
}

void SkyContextController::updateLocationStatusText()
{
    switch (m_locationSource) {
    case SkyContextLocationSource::CurrentDevice:
        setLocationStatusText("Location: Current device");
        return;
    case SkyContextLocationSource::City:
        if (!m_selectedCityDisplayText.isEmpty()) {
            setLocationStatusText(QString("Location: City - %1").arg(m_selectedCityDisplayText));
            return;
        }

        setLocationStatusText("Location: City");
        return;
    case SkyContextLocationSource::Custom:
        setLocationStatusText("Location: Custom coordinates");
        return;
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

bool SkyContextController::downloadingCatalog() const noexcept
{
    return m_catalogManager != nullptr && m_catalogManager->downloadingCatalog();
}

bool SkyContextController::catalogProcessing() const noexcept
{
    return m_catalogManager != nullptr && m_catalogManager->catalogProcessing();
}

QString SkyContextController::skyContextSummary() const
{
    QString bodyCountText = "n/a";
    if (m_catalogManager != nullptr && m_catalogManager->bodyCount() > 0U) {
        bodyCountText = QString::number(
            static_cast<qulonglong>(m_catalogManager->bodyCount())
        );
    }

    return QString(
        "UTC %1 %2 | Lat %3 | Lon %4 | Elev %5 m | Proj %6 | View Alt %7 Az %8 | FOV %9 | Bodies %10"
    ).arg(
        utcDateText(),
        utcTimeText(),
        latitudeText(),
        longitudeText(),
        elevationText(),
        projectionTypeText(),
        QString::number(m_viewCenterAltitudeDeg, 'f', 1),
        QString::number(m_viewCenterAzimuthDeg, 'f', 1),
        QString::number(m_viewFieldOfViewDeg, 'f', 1),
        bodyCountText
    );
}

const skygate::core::SkyContext& SkyContextController::skyContext() const noexcept
{
    return m_skyContext;
}

std::uint64_t SkyContextController::catalogRevision() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->catalogRevision() : 0U;
}

const skygate::ephemeris::IEphemerisEngine* SkyContextController::ephemerisEngine() const noexcept
{
    return m_catalogManager != nullptr ? m_catalogManager->ephemerisEngine() : nullptr;
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

void SkyContextController::setCatalogPresetIndex(const int catalogPresetIndex)
{
    if (m_catalogManager != nullptr) {
        m_catalogManager->setCatalogPresetIndex(catalogPresetIndex);
    }
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

void SkyContextController::clearSelectedCity()
{
    const bool hadSelectedCityId = !m_selectedCityId.isEmpty();
    const bool hadSelectedCityDisplayText = !m_selectedCityDisplayText.isEmpty();
    if (!hadSelectedCityId && !hadSelectedCityDisplayText) {
        return;
    }

    m_selectedCityId.clear();
    m_selectedCityDisplayText.clear();

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

    if (m_locationSource == nextLocationSource) {
        if (m_locationSource != SkyContextLocationSource::City) {
            clearSelectedCity();
        }
        updateLocationStatusText();
        return;
    }

    m_locationSource = nextLocationSource;
    if (m_locationSource != SkyContextLocationSource::City) {
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

    const bool cityIdChanged = m_selectedCityId != cityEntry->id;
    const QString displayText = cityEntry->displayText();
    const bool displayTextChanged = m_selectedCityDisplayText != displayText;

    m_selectedCityId = cityEntry->id;
    m_selectedCityDisplayText = displayText;
    m_locationSource = SkyContextLocationSource::City;

    if (cityIdChanged) {
        emit selectedCityIdChanged();
    }
    if (displayTextChanged) {
        emit selectedCityDisplayTextChanged();
    }
    emit locationSourceTextChanged();

    skygate::core::GeoLocation observer = m_skyContext.observer;
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
        if (m_locationSource == SkyContextLocationSource::City) {
            updateLocationStatusText();
        }
        return;
    }

    (void)applySelectedCityId(selectedCityId.trimmed());
}
