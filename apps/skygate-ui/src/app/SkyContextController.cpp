#include "SkyContextController.hpp"

#include "LocationCatalogModel.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkyLogging.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySettingsStore.hpp"
#include "SkyTimeController.hpp"

#include "SkyQtTimeCodec.hpp"
#include "UtcTimeCodec.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "EphemerisEngineFactory.hpp"
#include "engine/highprecision/DeltaTProvider.hpp"
#include "engine/highprecision/EarthOrientationProvider.hpp"
#include "engine/highprecision/LeapSecondProvider.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

using namespace skygate::ui::internal;

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisUpdateLog, "skygate.ephemeris.update")

using skygate::ephemeris::EphemerisCorrectionFlags;
using skygate::ephemeris::EphemerisEngineKind;

[[nodiscard]] qint64 floorMod(const qint64 numerator, const qint64 denominator) noexcept
{
    qint64 remainder = numerator % denominator;
    if (remainder < 0) {
        remainder += denominator;
    }
    return remainder;
}

[[nodiscard]] std::optional<skygate::ephemeris::AstronomicalEpoch>
astronomicalEpochFromUtcTime(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    const QDateTime normalizedUtcDateTime = SkyQtTimeCodec::toQDateTimeUtc(utcTime);
    const QDate date = normalizedUtcDateTime.date();
    const QTime time = normalizedUtcDateTime.time();
    const auto astronomicalYear = skygate::ephemeris::astronomicalYearFromHistoricalYear(date.year());
    if (!astronomicalYear.has_value()) {
        return std::nullopt;
    }

    return skygate::ephemeris::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = *astronomicalYear,
            .month = date.month(),
            .day = date.day(),
            .hour = time.hour(),
            .minute = time.minute(),
            .second = time.second(),
            .nanosecond = static_cast<std::uint32_t>(floorMod(
                              static_cast<qint64>(skygate::core::UtcTimeCodec::toEpochMicros(utcTime)), 1'000'000
                          ))
                          * 1000U,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
}

[[nodiscard]] EphemerisEngineKind engineKindFromIndex(const int index) noexcept
{
    return index == 1 ? EphemerisEngineKind::HighPrecision : EphemerisEngineKind::Simple;
}

[[nodiscard]] int engineKindIndex(const EphemerisEngineKind kind) noexcept
{
    return kind == EphemerisEngineKind::HighPrecision ? 1 : 0;
}

[[nodiscard]] QString cacheSizeText(const std::uint64_t bytes)
{
    if (bytes == 0U) {
        return QStringLiteral("0 MB");
    }

    constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;
    return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / kBytesPerMegabyte, 0, 'f', 1);
}

[[nodiscard]] QString sentenceFragment(QString text)
{
    text = text.trimmed();
    if (!text.isEmpty() && text.front().isUpper() && (text.size() == 1 || !text.at(1).isUpper())) {
        text.front() = text.front().toLower();
    }
    return text;
}

[[nodiscard]] QString displayDataStatus(QString statusText)
{
    statusText = statusText.trimmed();
    constexpr QLatin1StringView kInstalledPrefix("Installed: ");
    if (statusText.startsWith(kInstalledPrefix)) {
        return statusText.mid(kInstalledPrefix.size());
    }
    if (statusText.startsWith(QStringLiteral("Bundled fallback"))) {
        return QStringLiteral("Bundled");
    }
    return statusText;
}

[[nodiscard]] QString
supportDataStatusText(const QString& rawStatusText, const QString& availableVersion, const bool updateChecked)
{
    const QString displayStatus = displayDataStatus(rawStatusText);
    if (!updateChecked) {
        return displayStatus;
    }
    if (displayStatus == QStringLiteral("Bundled")) {
        return availableVersion.isEmpty() ? QStringLiteral("Bundled (latest)")
                                          : QStringLiteral("Bundled (available: %1)").arg(availableVersion);
    }
    if (!availableVersion.isEmpty() && availableVersion > displayStatus) {
        return QStringLiteral("%1 (available: %2)").arg(displayStatus, availableVersion);
    }
    return QStringLiteral("%1 (latest)").arg(displayStatus);
}

[[nodiscard]] QString supportDataVersion(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const skygate::ephemeris::EphemerisDataManifestAssetKind kind
)
{
    const skygate::ephemeris::EphemerisDataManifestProfile* profile = manifest.profile("support-data");
    if (profile == nullptr) {
        return {};
    }
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        if (asset != nullptr && asset->kind == kind) {
            return QString::fromStdString(asset->version).trimmed();
        }
    }
    return {};
}

[[nodiscard]] QString profileProgressName(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const skygate::ephemeris::EphemerisDataManifestProfile& profile
)
{
    for (const std::string& assetId : profile.assetIds) {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        if (asset != nullptr && asset->kind == skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel) {
            return QString::fromStdString(asset->version).trimmed();
        }
    }
    return sentenceFragment(
        profile.displayName.empty() ? QString::fromStdString(profile.id) : QString::fromStdString(profile.displayName)
    );
}

[[nodiscard]] EphemerisCorrectionFlags correctionPresetFlags(const int index) noexcept
{
    switch (index) {
    case 0:
        return EphemerisCorrectionFlags::Geometric;
    case 1:
        return EphemerisCorrectionFlags::Astrometric;
    case 2:
        return EphemerisCorrectionFlags::Apparent;
    default:
        return EphemerisCorrectionFlags::Topocentric;
    }
}

[[nodiscard]] QString correctionPresetId(const int index)
{
    switch (index) {
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

[[nodiscard]] int correctionPresetIndex(const EphemerisCorrectionFlags flags) noexcept
{
    const EphemerisCorrectionFlags baseFlags =
        skygate::ephemeris::withoutCorrectionFlags(flags, EphemerisCorrectionFlags::AtmosphericRefraction);
    if (baseFlags == EphemerisCorrectionFlags::Geometric) {
        return 0;
    }
    if (baseFlags == EphemerisCorrectionFlags::Astrometric) {
        return 1;
    }
    if (baseFlags == EphemerisCorrectionFlags::Apparent) {
        return 2;
    }
    return 3;
}

[[nodiscard]] EphemerisCorrectionFlags
correctionFlagsWithRefraction(const EphemerisCorrectionFlags baseFlags, const bool enabled) noexcept
{
    if (enabled) {
        return baseFlags | EphemerisCorrectionFlags::AtmosphericRefraction;
    }
    return skygate::ephemeris::withoutCorrectionFlags(baseFlags, EphemerisCorrectionFlags::AtmosphericRefraction);
}

[[nodiscard]] QString decimalText(const double value, const int precision)
{
    return QString::number(value, 'f', precision);
}

[[nodiscard]] QString safeEphemerisPathSegment(QString value)
{
    value = value.trimmed();
    QString result;
    result.reserve(value.size());
    for (const QChar character : value) {
        if (character.isLetterOrNumber() || character == QLatin1Char('-') || character == QLatin1Char('_')
            || character == QLatin1Char('.')) {
            result.push_back(character);
        } else {
            result.push_back(QLatin1Char('_'));
        }
    }
    while (result.contains(QStringLiteral(".."))) {
        result.replace(QStringLiteral(".."), QStringLiteral("."));
    }
    return result.isEmpty() || result == QStringLiteral(".") ? QStringLiteral("profile") : result;
}

[[nodiscard]] std::optional<double> boundedDouble(const QString& text, const double minimum, const double maximum)
{
    bool isValidNumber = false;
    const double value = text.trimmed().toDouble(&isValidNumber);
    if (!isValidNumber || value < minimum || value > maximum) {
        return std::nullopt;
    }
    return value;
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

[[nodiscard]] std::optional<skygate::ephemeris::EphemerisDateRange> activeTextAssetRange(
    const skygate::ephemeris::EphemerisDataManifest* manifest,
    const skygate::ephemeris::EphemerisDataManifestAssetKind kind,
    const QString& activeVersion
)
{
    if (manifest == nullptr || activeVersion.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const std::string version = activeVersion.trimmed().toStdString();
    for (const skygate::ephemeris::EphemerisDataManifestAsset& asset : manifest->assets) {
        if (asset.kind == kind && asset.version == version) {
            return asset.validityRange;
        }
    }

    return std::nullopt;
}

struct EphemerisProviderBundle final {
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
};

[[nodiscard]] EphemerisProviderBundle
ephemerisProvidersFromSnapshot(const std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot>& snapshot)
{
    EphemerisProviderBundle bundle;
    if (snapshot == nullptr) {
        return bundle;
    }

    const skygate::ephemeris::EarthOrientationDataLoadResult earthOrientationData =
        skygate::ephemeris::loadEarthOrientationDataFromSnapshot(*snapshot);
    if (earthOrientationData.isSuccess()) {
        bundle.earthOrientationProvider = earthOrientationData.provider;
    }

    const skygate::ephemeris::LeapSecondTableLoadResult leapSecondTable =
        skygate::ephemeris::loadLeapSecondTableFromSnapshot(*snapshot);
    const skygate::ephemeris::DeltaTDataLoadResult deltaTData =
        skygate::ephemeris::loadDeltaTDataFromSnapshot(*snapshot);
    if (leapSecondTable.isSuccess()) {
        skygate::ephemeris::TimeScaleServiceOptions timeScaleOptions;
        timeScaleOptions.allowDegradedLeapSecondFallback = true;
        timeScaleOptions.allowUt1DeltaTFallback = true;
        timeScaleOptions.earthOrientationSampleOptions.allowOutOfRangeNearestSampleFallback = true;
        timeScaleOptions.earthOrientationSampleOptions.allowMissingDataZeroFallback = true;
        timeScaleOptions.earthOrientationSampleOptions.degradePredictedData = false;
        bundle.timeScaleService = std::make_shared<skygate::ephemeris::LeapSecondTimeScaleService>(
            leapSecondTable.provider,
            timeScaleOptions,
            bundle.earthOrientationProvider,
            deltaTData.isSuccess() ? deltaTData.provider : nullptr
        );
    }

    return bundle;
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
      m_ephemerisUpdateResourceRoot(initializationOptions.ephemerisFactoryInputs.updateResourceRoot),
      m_ephemerisWritableCacheRoot(
          initializationOptions.ephemerisFactoryInputs.writableCacheRoot.trimmed().isEmpty()
              ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/ephemeris-data")
              : initializationOptions.ephemerisFactoryInputs.writableCacheRoot.trimmed()
      ),
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
    if (m_ephemerisDataManager != nullptr) {
        m_ephemerisDataManager->setBundledFallbackData(m_ephemerisDataManifest, m_ephemerisUpdateResourceRoot);
    }
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
    if (m_timeline.live() && !m_liveClock.isRunning()) {
        m_liveClock.start(m_location.utcTime());
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

int SkyContextController::ephemerisEngineKindIndex() const noexcept
{
    return engineKindIndex(m_ephemerisEngineKind);
}

int SkyContextController::ephemerisCorrectionPresetIndex() const noexcept
{
    return correctionPresetIndex(m_ephemerisEngineOptions.correctionFlags);
}

bool SkyContextController::ephemerisRefractionEnabled() const noexcept
{
    return m_ephemerisEngineOptions.enableAtmosphericRefraction;
}

QString SkyContextController::ephemerisAtmosphericPressureText() const
{
    return decimalText(m_ephemerisEngineOptions.atmosphericPressureHpa, 2);
}

QString SkyContextController::ephemerisAtmosphericTemperatureText() const
{
    return decimalText(m_ephemerisEngineOptions.atmosphericTemperatureC, 1);
}

QString SkyContextController::ephemerisRelativeHumidityText() const
{
    return decimalText(m_ephemerisEngineOptions.relativeHumidity * 100.0, 1);
}

QString SkyContextController::ephemerisWavelengthText() const
{
    return decimalText(m_ephemerisEngineOptions.observingWavelengthMicrometers, 2);
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
    if (!m_ephemerisDataOperationStatusText.isEmpty()) {
        return m_ephemerisDataOperationStatusText;
    }
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->statusText() : QString();
}

QString SkyContextController::ephemerisShortRangeKernelStatusText() const
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->shortRangeKernelStatusText() : QString();
}

QString SkyContextController::ephemerisLongRangeKernelStatusText() const
{
    return m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->longRangeKernelStatusText() : QString();
}

QString SkyContextController::ephemerisEarthOrientationStatusText() const
{
    return supportDataStatusText(
        m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->earthOrientationStatusText() : QString(),
        m_availableEarthOrientationVersion,
        m_ephemerisSupportDataUpdateChecked
    );
}

QString SkyContextController::ephemerisLeapSecondStatusText() const
{
    return supportDataStatusText(
        m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->leapSecondStatusText() : QString(),
        m_availableLeapSecondVersion,
        m_ephemerisSupportDataUpdateChecked
    );
}

QString SkyContextController::ephemerisDeltaTStatusText() const
{
    return supportDataStatusText(
        m_ephemerisDataManager != nullptr ? m_ephemerisDataManager->deltaTStatusText() : QString(),
        m_availableDeltaTVersion,
        m_ephemerisSupportDataUpdateChecked
    );
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
    return !m_ephemerisDataUpdateInProgress && activeEphemerisDataManifest() != nullptr
           && !m_ephemerisUpdateResourceRoot.trimmed().isEmpty() && !m_ephemerisWritableCacheRoot.trimmed().isEmpty();
}

bool SkyContextController::ephemerisDataUpdateInProgress() const noexcept
{
    return m_ephemerisDataUpdateInProgress;
}

double SkyContextController::ephemerisDataUpdateProgress() const noexcept
{
    return m_ephemerisDataUpdateProgress;
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

skygate::ephemeris::EphemerisEngineKind SkyContextController::activeEphemerisEngineKind() const noexcept
{
    return m_ephemerisEngine != nullptr ? m_ephemerisEngine->kind() : m_ephemerisEngineKind;
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
    if (m_ephemerisDataManager != nullptr) {
        const SkySettingsStore::EphemerisDataCacheSnapshot activeCache = m_ephemerisDataManager->activeCacheSnapshot();
        const skygate::ephemeris::EphemerisDataManifest* manifest = activeEphemerisDataManifest();
        context.earthOrientationDataRange = activeTextAssetRange(
            manifest,
            skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData,
            activeCache.installedEarthOrientationVersion
        );
        context.leapSecondTableRange = activeTextAssetRange(
            manifest,
            skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable,
            activeCache.installedLeapSecondTableVersion
        );
        context.deltaTDataRange = activeTextAssetRange(
            manifest,
            skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData,
            activeCache.installedDeltaTDataVersion
        );
    }

    if (const auto epoch = astronomicalEpochFromUtcTime(skyContext.utcTime); epoch.has_value()) {
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
    const std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot =
        activeEphemerisDataSnapshot();
    const EphemerisProviderBundle snapshotProviders =
        m_ephemerisEngineKind == skygate::ephemeris::EphemerisEngineKind::HighPrecision
                && (m_ephemerisTimeScaleService == nullptr || m_ephemerisEarthOrientationProvider == nullptr)
            ? ephemerisProvidersFromSnapshot(activeDataSnapshot)
            : EphemerisProviderBundle{};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = m_ephemerisEngineKind;
    request.catalogBodies = catalogBodies();
    request.options = m_ephemerisEngineOptions;
    request.options.engineKind = m_ephemerisEngineKind;
    const skygate::ephemeris::EphemerisDataManifest* dataManifest = activeEphemerisDataManifest();
    request.dataSetManifest = dataManifest != nullptr ? &dataManifest->dataSetInfo : m_ephemerisDataSetManifest;
    request.dataManifest = dataManifest;
    request.activeDataSnapshot = activeDataSnapshot;
    request.timeScaleService =
        m_ephemerisTimeScaleService != nullptr ? m_ephemerisTimeScaleService : snapshotProviders.timeScaleService;
    request.earthOrientationProvider = m_ephemerisEarthOrientationProvider != nullptr
                                           ? m_ephemerisEarthOrientationProvider
                                           : snapshotProviders.earthOrientationProvider;
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

void SkyContextController::applyEphemerisUserSettings(const SkySettingsStore::EphemerisUserSettingsSnapshot& settings)
{
    skygate::ephemeris::EphemerisEngineOptions nextOptions = m_ephemerisEngineOptions;
    nextOptions.engineKind = settings.engineKind;
    nextOptions.correctionFlags = settings.correctionFlags;
    nextOptions.fallbackToSimpleEngine = settings.fallbackToSimpleEngine;
    nextOptions.enableAtmosphericRefraction = settings.refractionEnabled;
    nextOptions.atmosphericPressureHpa = settings.atmosphericPressureHpa;
    nextOptions.atmosphericTemperatureC = settings.atmosphericTemperatureC;
    nextOptions.relativeHumidity = settings.relativeHumidity;
    nextOptions.observingWavelengthMicrometers = settings.observingWavelengthMicrometers;

    if (m_ephemerisEngineKind == settings.engineKind && m_ephemerisEngineOptions.engineKind == nextOptions.engineKind
        && m_ephemerisEngineOptions.correctionFlags == nextOptions.correctionFlags
        && m_ephemerisEngineOptions.fallbackToSimpleEngine == nextOptions.fallbackToSimpleEngine
        && m_ephemerisEngineOptions.enableAtmosphericRefraction == nextOptions.enableAtmosphericRefraction
        && m_ephemerisEngineOptions.atmosphericPressureHpa == nextOptions.atmosphericPressureHpa
        && m_ephemerisEngineOptions.atmosphericTemperatureC == nextOptions.atmosphericTemperatureC
        && m_ephemerisEngineOptions.relativeHumidity == nextOptions.relativeHumidity
        && m_ephemerisEngineOptions.observingWavelengthMicrometers == nextOptions.observingWavelengthMicrometers) {
        m_ephemerisUserSettings = settings;
        return;
    }

    m_ephemerisUserSettings = settings;
    m_ephemerisEngineKind = settings.engineKind;
    m_ephemerisEngineOptions = nextOptions;
    ++m_ephemerisOptionsRevision;
    rebuildEphemerisEngine();
    emit ephemerisSettingsChanged();
    emit skyContextChanged();
}

void SkyContextController::setEphemerisEngineKindIndex(const int engineKindIndex)
{
    if (engineKindIndex < 0 || engineKindIndex > 1) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.engineKind = engineKindFromIndex(engineKindIndex);
    if (settings.engineKind == EphemerisEngineKind::Simple) {
        settings.correctionPresetId = correctionPresetId(0);
        settings.correctionFlags = EphemerisCorrectionFlags::NoCorrections;
        settings.refractionEnabled = false;
    } else if (
        m_ephemerisEngineKind == EphemerisEngineKind::Simple
        && m_ephemerisEngineOptions.correctionFlags == EphemerisCorrectionFlags::NoCorrections
    ) {
        settings.correctionPresetId = correctionPresetId(3);
        settings.refractionEnabled = true;
        settings.correctionFlags = EphemerisCorrectionFlags::ApparentTopocentric;
    }
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisCorrectionPresetIndex(const int correctionPresetIndex)
{
    if (correctionPresetIndex < 0 || correctionPresetIndex > 3) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.correctionPresetId = correctionPresetId(correctionPresetIndex);
    settings.correctionFlags =
        correctionFlagsWithRefraction(correctionPresetFlags(correctionPresetIndex), settings.refractionEnabled);
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisRefractionEnabled(const bool enabled)
{
    auto settings = m_ephemerisUserSettings;
    settings.refractionEnabled = enabled;
    settings.correctionFlags =
        correctionFlagsWithRefraction(correctionPresetFlags(ephemerisCorrectionPresetIndex()), enabled);
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisAtmosphericPressureText(const QString& pressureText)
{
    const std::optional<double> pressure = boundedDouble(pressureText, 0.0, 2000.0);
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
    const std::optional<double> temperature = boundedDouble(temperatureText, -100.0, 80.0);
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
    const std::optional<double> humidityPercent = boundedDouble(humidityText, 0.0, 100.0);
    if (!humidityPercent.has_value()) {
        emit ephemerisSettingsChanged();
        return;
    }

    auto settings = m_ephemerisUserSettings;
    settings.relativeHumidity = *humidityPercent / 100.0;
    applyEphemerisUserSettings(settings);
}

void SkyContextController::setEphemerisWavelengthText(const QString& wavelengthText)
{
    const std::optional<double> wavelength = boundedDouble(wavelengthText, 0.1, 100.0);
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

void SkyContextController::setEphemerisDataOperationStatusText(QString statusText)
{
    statusText = statusText.trimmed();
    if (m_ephemerisDataOperationStatusText == statusText) {
        return;
    }

    m_ephemerisDataOperationStatusText = std::move(statusText);
    emit ephemerisDataStatusTextChanged();
}

void SkyContextController::setEphemerisDataUpdateProgress(const double progress) noexcept
{
    const double boundedProgress = std::clamp(progress, 0.0, 1.0);
    if (m_ephemerisDataUpdateProgress == boundedProgress) {
        return;
    }

    m_ephemerisDataUpdateProgress = boundedProgress;
    emit ephemerisDataStatusTextChanged();
}

const skygate::ephemeris::EphemerisDataManifest* SkyContextController::activeEphemerisDataManifest() const noexcept
{
    return m_refreshedEphemerisDataManifest.has_value() ? &*m_refreshedEphemerisDataManifest : m_ephemerisDataManifest;
}

bool SkyContextController::refreshEphemerisDataManifest(const QString& stagedRoot)
{
    const QString manifestUrl = m_ephemerisUserSettings.updateManifestUrl.trimmed();
    if (manifestUrl.isEmpty()) {
        return activeEphemerisDataManifest() != nullptr;
    }
    if (m_ephemerisDataManager == nullptr) {
        return false;
    }

    skygate::ephemeris::EphemerisDataManifestAsset manifestAsset;
    manifestAsset.id = "manifest";
    manifestAsset.kind = skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData;
    manifestAsset.profileId = "manifest";
    manifestAsset.version = "manifest";
    manifestAsset.sourceUrl = manifestUrl.toStdString();
    manifestAsset.relativePath = "manifest.json";

    SkyEphemerisDataManager::StagedUpdateDownloadRequest request;
    request.asset = &manifestAsset;
    request.sourceUrl = manifestUrl;
    request.stagedResourceRoot = stagedRoot + QStringLiteral("/manifest");
    request.cancellationRequested = [this] {
        return m_ephemerisDataManager != nullptr && m_ephemerisDataManager->updateCancellationRequested();
    };

    const SkyEphemerisDataManager::StagedUpdateDownloadResult downloadResult =
        m_ephemerisDataManager->stageEphemerisUpdateAsset(request);
    if (!downloadResult.isSuccess()) {
        const QString diagnostic = downloadResult.diagnostics.empty() ? QStringLiteral("manifest download failed")
                                                                      : downloadResult.diagnostics.front();
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: %1").arg(diagnostic));
        return false;
    }

    QFile manifestFile(downloadResult.stagedPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(skygateEphemerisUpdateLog).noquote() << "Unable to open staged ephemeris update manifest"
                                                       << downloadResult.stagedPath << manifestFile.errorString();
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Unable to open update manifest"));
        return false;
    }

    const QByteArray payload = manifestFile.readAll();
    skygate::ephemeris::EphemerisDataManifestParseResult parseResult = skygate::ephemeris::parseEphemerisDataManifest(
        std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()))
    );
    if (!parseResult.isSuccess()) {
        const QString diagnostic = parseResult.diagnostics.empty()
                                       ? QStringLiteral("manifest parse failed")
                                       : QString::fromStdString(parseResult.diagnostics.front());
        qCWarning(skygateEphemerisUpdateLog).noquote() << "Unable to parse ephemeris update manifest:" << diagnostic;
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: %1").arg(diagnostic));
        return false;
    }

    m_refreshedEphemerisDataManifest = std::move(parseResult.manifest);
    return true;
}

bool SkyContextController::clearEphemerisDataCache()
{
    const bool cleared = m_ephemerisDataManager != nullptr && m_ephemerisDataManager->clearInstalledDataCache();
    if (cleared) {
        setEphemerisDataOperationStatusText({});
    }
    return cleared;
}

bool SkyContextController::clearPlanetaryKernelCache()
{
    const bool cleared = m_ephemerisDataManager != nullptr && m_ephemerisDataManager->clearPlanetaryKernelCache();
    if (cleared) {
        setEphemerisDataOperationStatusText({});
        emit ephemerisDataStatusTextChanged();
    }
    return cleared;
}

bool SkyContextController::clearSupportDataCache()
{
    const bool cleared = m_ephemerisDataManager != nullptr && m_ephemerisDataManager->clearSupportDataCache();
    if (cleared) {
        m_ephemerisSupportDataUpdateChecked = false;
        m_availableEarthOrientationVersion.clear();
        m_availableLeapSecondVersion.clear();
        m_availableDeltaTVersion.clear();
        setEphemerisDataOperationStatusText({});
        emit ephemerisDataStatusTextChanged();
    }
    return cleared;
}

bool SkyContextController::checkEphemerisSupportDataUpdates()
{
    if (m_ephemerisDataUpdateInProgress) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update already in progress"));
        return false;
    }
    if (activeEphemerisDataManifest() == nullptr) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update manifest unavailable"));
        return false;
    }
    if (m_ephemerisWritableCacheRoot.trimmed().isEmpty()) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Writable cache unavailable"));
        return false;
    }

    const QString stagedRoot = m_ephemerisWritableCacheRoot + QStringLiteral("/staging/support-data-info");
    QDir stagingDirectory(stagedRoot);
    if (stagingDirectory.exists() && !stagingDirectory.removeRecursively()) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to clear ephemeris support-data staging directory" << stagedRoot;
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Unable to clear update staging area"));
        return false;
    }
    if (!QDir().mkpath(stagedRoot)) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to create ephemeris support-data staging directory" << stagedRoot;
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Unable to create update staging area"));
        return false;
    }

    if (m_ephemerisDataManager != nullptr) {
        m_ephemerisDataManager->clearUpdateCancellation();
    }
    m_ephemerisDataUpdateInProgress = true;
    setEphemerisDataUpdateProgress(0.0);
    setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Checking time and Earth data..."));
    const bool refreshed = refreshEphemerisDataManifest(stagedRoot);
    m_ephemerisDataUpdateInProgress = false;
    if (m_ephemerisDataManager != nullptr) {
        m_ephemerisDataManager->clearUpdateCancellation();
    }
    setEphemerisDataUpdateProgress(refreshed ? 1.0 : 0.0);
    if (!refreshed) {
        emit ephemerisDataStatusTextChanged();
        return false;
    }

    const skygate::ephemeris::EphemerisDataManifest* manifest = activeEphemerisDataManifest();
    if (manifest == nullptr) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update manifest unavailable"));
        emit ephemerisDataStatusTextChanged();
        return false;
    }

    m_availableEarthOrientationVersion =
        supportDataVersion(*manifest, skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData);
    m_availableLeapSecondVersion =
        supportDataVersion(*manifest, skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable);
    m_availableDeltaTVersion =
        supportDataVersion(*manifest, skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData);
    m_ephemerisSupportDataUpdateChecked = true;
    setEphemerisDataOperationStatusText({});
    emit ephemerisDataStatusTextChanged();
    return true;
}

void SkyContextController::cancelActiveDownload()
{
    if (m_ephemerisDataUpdateInProgress && m_ephemerisDataManager != nullptr) {
        m_ephemerisDataManager->requestUpdateCancellation();
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Canceling download..."));
    }
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
    if (m_ephemerisDataManager == nullptr) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update manager unavailable"));
        return false;
    }
    if (m_ephemerisDataUpdateInProgress) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update already in progress"));
        return false;
    }
    if (activeEphemerisDataManifest() == nullptr) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update manifest unavailable"));
        return false;
    }
    if (m_ephemerisWritableCacheRoot.trimmed().isEmpty()) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Writable cache unavailable"));
        return false;
    }

    const QString normalizedProfileId = profileIdText.trimmed();
    if (normalizedProfileId.isEmpty()) {
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Update profile unavailable"));
        return false;
    }

    const QString stagedRoot =
        m_ephemerisWritableCacheRoot + QStringLiteral("/staging/") + safeEphemerisPathSegment(normalizedProfileId);
    QDir stagingDirectory(stagedRoot);
    if (stagingDirectory.exists() && !stagingDirectory.removeRecursively()) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to clear ephemeris profile staging directory" << stagedRoot;
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Unable to clear update staging area"));
        return false;
    }
    if (!QDir().mkpath(stagedRoot)) {
        qCWarning(skygateEphemerisUpdateLog).noquote()
            << "Unable to create ephemeris profile staging directory" << stagedRoot;
        setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Unable to create update staging area"));
        return false;
    }

    m_ephemerisDataUpdateInProgress = true;
    if (m_ephemerisDataManager != nullptr) {
        m_ephemerisDataManager->clearUpdateCancellation();
    }
    setEphemerisDataUpdateProgress(0.0);
    emit ephemerisDataStatusTextChanged();
    const auto finishUpdate = [this](const bool success, QString statusText) {
        if (!success && !statusText.trimmed().isEmpty()) {
            qCWarning(skygateEphemerisUpdateLog).noquote() << statusText;
        }
        m_ephemerisDataUpdateInProgress = false;
        if (m_ephemerisDataManager != nullptr) {
            m_ephemerisDataManager->clearUpdateCancellation();
        }
        setEphemerisDataUpdateProgress(success ? 1.0 : 0.0);
        setEphemerisDataOperationStatusText(std::move(statusText));
        emit ephemerisDataStatusTextChanged();
        return success;
    };
    setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Checking update manifest..."));
    if (!refreshEphemerisDataManifest(stagedRoot)) {
        return finishUpdate(false, m_ephemerisDataOperationStatusText);
    }

    const skygate::ephemeris::EphemerisDataManifest* updateManifest = activeEphemerisDataManifest();
    if (updateManifest == nullptr) {
        return finishUpdate(false, QStringLiteral("Ephemeris data: Update manifest unavailable"));
    }

    const std::string profileId = normalizedProfileId.toStdString();
    const skygate::ephemeris::EphemerisDataManifestProfile* profile = updateManifest->profile(profileId);
    if (profile == nullptr) {
        return finishUpdate(false, QStringLiteral("Ephemeris data: Update profile unavailable"));
    }

    std::uint64_t totalExpectedBytes = 0U;
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = updateManifest->asset(assetId);
        if (asset != nullptr && asset->compression.uncompressedSizeBytes.has_value()) {
            totalExpectedBytes += *asset->compression.uncompressedSizeBytes;
        }
    }
    std::uint64_t completedBytes = 0U;
    const QString progressName = profileProgressName(*updateManifest, *profile);
    setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Downloading %1...").arg(progressName));

    SkyEphemerisDataManager::StagedUpdateActivationRequest request;
    request.manifest = updateManifest;
    request.profileId = normalizedProfileId;
    request.stagedResourceRoot = stagedRoot;
    request.writableCacheRoot = m_ephemerisWritableCacheRoot;
    request.revisionToken = QString::fromStdString(profile->id);
    for (const std::string& assetId : profile->assetIds) {
        const skygate::ephemeris::EphemerisDataManifestAsset* asset = updateManifest->asset(assetId);
        if (asset == nullptr) {
            return finishUpdate(false, QStringLiteral("Ephemeris data: Update profile references a missing asset"));
        }

        SkyEphemerisDataManager::StagedUpdateDownloadRequest downloadRequest;
        downloadRequest.asset = asset;
        downloadRequest.sourceUrl = QString::fromStdString(asset->sourceUrl);
        if (downloadRequest.sourceUrl.trimmed().isEmpty()) {
            downloadRequest.sourceResourceRoot = m_ephemerisUpdateResourceRoot;
        }
        downloadRequest.stagedResourceRoot = stagedRoot;
        downloadRequest.cancellationRequested = [this] {
            return m_ephemerisDataManager != nullptr && m_ephemerisDataManager->updateCancellationRequested();
        };
        downloadRequest.progressHandler =
            [this,
             completedBytes,
             totalExpectedBytes](const std::uint64_t stagedBytes, const std::optional<std::uint64_t> totalBytes) {
                const std::uint64_t denominator =
                    totalExpectedBytes > 0U ? totalExpectedBytes : completedBytes + totalBytes.value_or(stagedBytes);
                if (denominator > 0U) {
                    setEphemerisDataUpdateProgress(
                        static_cast<double>(std::min(completedBytes + stagedBytes, denominator))
                        / static_cast<double>(denominator)
                    );
                }
            };

        const SkyEphemerisDataManager::StagedUpdateDownloadResult downloadResult =
            m_ephemerisDataManager->stageEphemerisUpdateAsset(downloadRequest);
        if (!downloadResult.isSuccess()) {
            const QString diagnostic = downloadResult.diagnostics.empty() ? QStringLiteral("asset download failed")
                                                                          : downloadResult.diagnostics.front();
            return finishUpdate(false, QStringLiteral("Ephemeris data: %1").arg(diagnostic));
        }
        completedBytes += asset->compression.uncompressedSizeBytes.value_or(downloadResult.stagedBytes);
        if (totalExpectedBytes > 0U) {
            setEphemerisDataUpdateProgress(
                static_cast<double>(std::min(completedBytes, totalExpectedBytes))
                / static_cast<double>(totalExpectedBytes)
            );
        }
        request.requiredKinds.push_back(asset->kind);
        auto& component = request.expectedComponents.emplace_back(asset->id, asset->kind);
        component.expectedVersion = asset->version;
        component.requiredValidityRange = asset->validityRange;
    }

    setEphemerisDataOperationStatusText(QStringLiteral("Ephemeris data: Verifying %1...").arg(progressName));
    const SkyEphemerisDataManager::StagedUpdateActivationResult activationResult =
        m_ephemerisDataManager->activateVerifiedStagedUpdateSet(request);
    const bool activated = activationResult.isSuccess();
    if (activated) {
        m_ephemerisUserSettings.preferredDataProfileId = normalizedProfileId;
        if (normalizedProfileId == QStringLiteral("support-data")) {
            m_ephemerisSupportDataUpdateChecked = false;
            m_availableEarthOrientationVersion.clear();
            m_availableLeapSecondVersion.clear();
            m_availableDeltaTVersion.clear();
        }
        return finishUpdate(true, {});
    }

    const QString diagnostic = activationResult.diagnostics.empty() ? QStringLiteral("activation failed")
                                                                    : activationResult.diagnostics.front();
    return finishUpdate(false, QStringLiteral("Ephemeris data: %1").arg(diagnostic));
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
