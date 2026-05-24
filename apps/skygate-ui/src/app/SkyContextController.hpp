#pragma once

#include "GeoLocation.hpp"
#include "ProjectionType.hpp"
#include "SkyContext.hpp"
#include "UtcTimePoint.hpp"
#include "SystemTimeSource.hpp"
#include "Types.hpp"
#include "catalog/constellation/ConstellationData.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "catalog/IStarCatalog.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyContextDomainControllers.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkySettingsStore.hpp"
#include "SkyLiveClock.hpp"
#include "SkyTimeController.hpp"

#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
class IEarthOrientationProvider;
class IEphemerisDataSnapshot;
class IEphemerisDiagnosticsSink;
class ITimeScaleService;
namespace highprecision {
class ICalcephKernelRuntime;
}
}  // namespace skygate::ephemeris

class QDateTime;
class QGeoPositionInfo;
class QGeoPositionInfoSource;
class LocationCatalogModel;
class SkyCatalogManager;
class SkyEphemerisDataManager;
class SkyObjectSearchModel;
class SkyOverlayLayerSettings;

class SkyContextController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* time READ timeController CONSTANT)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(
        bool timelineToolbarCollapsed
        READ timelineToolbarCollapsed
        WRITE setTimelineToolbarCollapsed
        NOTIFY timelineToolbarCollapsedChanged
    )
    Q_PROPERTY(
        bool searchToolbarCollapsed
        READ searchToolbarCollapsed
        WRITE setSearchToolbarCollapsed
        NOTIFY searchToolbarCollapsedChanged
    )
    Q_PROPERTY(double speedMultiplier READ speedMultiplier WRITE setSpeedMultiplier NOTIFY speedMultiplierChanged)
    Q_PROPERTY(int stepSeconds READ stepSeconds WRITE setStepSeconds NOTIFY stepSecondsChanged)
    Q_PROPERTY(double magnitudeCutoff READ magnitudeCutoff WRITE setMagnitudeCutoff NOTIFY magnitudeCutoffChanged)
    Q_PROPERTY(double viewCenterAltitudeDeg READ viewCenterAltitudeDeg NOTIFY viewDirectionChanged)
    Q_PROPERTY(double viewCenterAzimuthDeg READ viewCenterAzimuthDeg NOTIFY viewDirectionChanged)
    Q_PROPERTY(QString latitudeText READ latitudeText NOTIFY latitudeTextChanged)
    Q_PROPERTY(QString longitudeText READ longitudeText NOTIFY longitudeTextChanged)
    Q_PROPERTY(QString elevationText READ elevationText NOTIFY elevationTextChanged)
    Q_PROPERTY(
        QString locationSourceText
        READ locationSourceText
        WRITE setLocationSourceText
        NOTIFY locationSourceTextChanged
    )
    Q_PROPERTY(QStringList locationSourceOptions READ locationSourceOptions CONSTANT)
    Q_PROPERTY(QString selectedCityId READ selectedCityId WRITE setSelectedCityId NOTIFY selectedCityIdChanged)
    Q_PROPERTY(
        QString selectedCityDisplayText
        READ selectedCityDisplayText
        NOTIFY selectedCityDisplayTextChanged
    )
    Q_PROPERTY(QAbstractItemModel* cityCatalogModel READ cityCatalogModel CONSTANT)
    Q_PROPERTY(QString projectionTypeText READ projectionTypeText NOTIFY projectionTypeChanged)
    Q_PROPERTY(QString themeId READ themeId WRITE setThemeId NOTIFY themeChanged)
    Q_PROPERTY(QVariantList themeOptions READ themeOptions NOTIFY themeOptionsChanged)
    Q_PROPERTY(QObject* theme READ theme CONSTANT)
    Q_PROPERTY(QObject* overlayLayers READ overlayLayers CONSTANT)
    Q_PROPERTY(bool logToTerminal READ logToTerminal WRITE setLogToTerminal NOTIFY loggingChanged)
    Q_PROPERTY(bool logToFile READ logToFile WRITE setLogToFile NOTIFY loggingChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath WRITE setLogFilePath NOTIFY loggingChanged)
    Q_PROPERTY(int ephemerisEngineKindIndex READ ephemerisEngineKindIndex WRITE setEphemerisEngineKindIndex NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(int ephemerisCorrectionPresetIndex READ ephemerisCorrectionPresetIndex WRITE setEphemerisCorrectionPresetIndex NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(bool ephemerisRefractionEnabled READ ephemerisRefractionEnabled WRITE setEphemerisRefractionEnabled NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(QString ephemerisAtmosphericPressureText READ ephemerisAtmosphericPressureText WRITE setEphemerisAtmosphericPressureText NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(QString ephemerisAtmosphericTemperatureText READ ephemerisAtmosphericTemperatureText WRITE setEphemerisAtmosphericTemperatureText NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(QString ephemerisRelativeHumidityText READ ephemerisRelativeHumidityText WRITE setEphemerisRelativeHumidityText NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(QString ephemerisWavelengthText READ ephemerisWavelengthText WRITE setEphemerisWavelengthText NOTIFY ephemerisSettingsChanged)
    Q_PROPERTY(QString locationStatusText READ locationStatusText NOTIFY locationStatusTextChanged)
    Q_PROPERTY(QString catalogStatusText READ catalogStatusText NOTIFY catalogStatusTextChanged)
    Q_PROPERTY(
        QString ephemerisDataStatusText
        READ ephemerisDataStatusText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisShortRangeKernelStatusText
        READ ephemerisShortRangeKernelStatusText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisLongRangeKernelStatusText
        READ ephemerisLongRangeKernelStatusText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisEarthOrientationStatusText
        READ ephemerisEarthOrientationStatusText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisLeapSecondStatusText
        READ ephemerisLeapSecondStatusText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisDeltaTStatusText
        READ ephemerisDeltaTStatusText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisDataLastUpdateResultText
        READ ephemerisDataLastUpdateResultText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisPlanetaryKernelCacheSizeText
        READ ephemerisPlanetaryKernelCacheSizeText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString ephemerisSupportDataCacheSizeText
        READ ephemerisSupportDataCacheSizeText
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        bool ephemerisDataOnlineUpdatesEnabled
        READ ephemerisDataOnlineUpdatesEnabled
        WRITE setEphemerisDataOnlineUpdatesEnabled
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        bool ephemerisDataUpdateEnabled
        READ ephemerisDataUpdateEnabled
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        bool ephemerisDataUpdateInProgress
        READ ephemerisDataUpdateInProgress
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        double ephemerisDataUpdateProgress
        READ ephemerisDataUpdateProgress
        NOTIFY ephemerisDataStatusTextChanged
    )
    Q_PROPERTY(
        QString catalogDatasetInfoText
        READ catalogDatasetInfoText
        NOTIFY catalogDatasetInfoTextChanged
    )
    Q_PROPERTY(
        QString deepSkyCatalogInfoText
        READ deepSkyCatalogInfoText
        NOTIFY deepSkyCatalogInfoTextChanged
    )
    Q_PROPERTY(QAbstractItemModel* objectSearchModel READ objectSearchModel CONSTANT)
    Q_PROPERTY(bool downloadingCatalog READ downloadingCatalog NOTIFY downloadingCatalogChanged)
    Q_PROPERTY(bool catalogProcessing READ catalogProcessing NOTIFY catalogProcessingChanged)
    Q_PROPERTY(
        QString selectedSearchTargetKind
        READ selectedSearchTargetKind
        NOTIFY selectedSearchTargetChanged
    )
    Q_PROPERTY(
        QString selectedSearchTargetId
        READ selectedSearchTargetId
        NOTIFY selectedSearchTargetChanged
    )
    Q_PROPERTY(bool hasTrackedTarget READ hasTrackedTarget NOTIFY trackedTargetChanged)
    Q_PROPERTY(
        QString trackedTargetKind
        READ trackedTargetKind
        NOTIFY trackedTargetChanged
    )
    Q_PROPERTY(
        QString trackedTargetId
        READ trackedTargetId
        NOTIFY trackedTargetChanged
    )
    Q_PROPERTY(
        QString trackedTargetDisplayText
        READ trackedTargetDisplayText
        NOTIFY trackedTargetChanged
    )
    Q_PROPERTY(QVariantMap nightConditions READ nightConditions NOTIFY nightConditionsChanged)
    Q_PROPERTY(
        QString nightConditionsIconKind
        READ nightConditionsIconKind
        NOTIFY nightConditionsChanged
    )

public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationLabelRef = skygate::ephemeris::ConstellationLabelRef;

public:
    struct EphemerisRequestContext final {
        skygate::ephemeris::EphemerisRequest request;
        std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot> activeDataSnapshot;
        std::uint64_t engineOptionsRevision = 0U;
        std::uint64_t ephemerisDataRevision = 0U;
        std::uint64_t earthOrientationDataRevision = 0U;
        std::uint64_t leapSecondDataRevision = 0U;
        std::uint64_t catalogRevision = 0U;
        std::optional<skygate::ephemeris::EphemerisDateRange> earthOrientationDataRange;
        std::optional<skygate::ephemeris::EphemerisDateRange> leapSecondTableRange;
        std::optional<skygate::ephemeris::EphemerisDateRange> deltaTDataRange;
    };

    struct InitializationOptions final {
        struct EphemerisFactoryInputs final {
            const skygate::ephemeris::EphemerisDataSetInfo* dataSetManifest = nullptr;
            const skygate::ephemeris::EphemerisDataManifest* dataManifest = nullptr;
            std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
            std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
            std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernelRuntime> calcephKernelRuntime;
            skygate::ephemeris::IEphemerisDiagnosticsSink* diagnosticsSink = nullptr;
            QString updateResourceRoot;
            QString writableCacheRoot;
        };

        bool loadSettings = true;
        bool initializeLocation = true;
        QGeoPositionInfoSource* positionSource = nullptr;
        bool requestLocationPermission = true;
        const skygate::core::ITimeSource* timeSource = nullptr;
        bool rebuildEphemerisEngineOnStartup = true;
        EphemerisFactoryInputs ephemerisFactoryInputs;
    };

    explicit SkyContextController(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog = nullptr,
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine = nullptr,
        QObject* parent = nullptr
    );
    SkyContextController(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
        InitializationOptions initializationOptions,
        QObject* parent
    );
    ~SkyContextController() override;

    [[nodiscard]] bool live() const noexcept;
    [[nodiscard]] bool timelineToolbarCollapsed() const noexcept;
    [[nodiscard]] bool searchToolbarCollapsed() const noexcept;
    [[nodiscard]] double speedMultiplier() const noexcept;
    [[nodiscard]] int stepSeconds() const noexcept;
    [[nodiscard]] double magnitudeCutoff() const noexcept;
    [[nodiscard]] double viewCenterAltitudeDeg() const noexcept;
    [[nodiscard]] double viewCenterAzimuthDeg() const noexcept;
    [[nodiscard]] QString utcDateText() const;
    [[nodiscard]] QString utcTimeText() const;
    [[nodiscard]] QString latitudeText() const;
    [[nodiscard]] QString longitudeText() const;
    [[nodiscard]] QString elevationText() const;
    [[nodiscard]] QString locationSourceText() const;
    [[nodiscard]] QStringList locationSourceOptions() const;
    [[nodiscard]] QString selectedCityId() const;
    [[nodiscard]] QString selectedCityDisplayText() const;
    [[nodiscard]] QAbstractItemModel* cityCatalogModel() const noexcept;
    [[nodiscard]] QString projectionTypeText() const;
    [[nodiscard]] QString themeId() const;
    [[nodiscard]] QVariantList themeOptions() const;
    [[nodiscard]] QObject* theme() const noexcept;
    [[nodiscard]] SkyTimeController* timeController() const noexcept;
    [[nodiscard]] QObject* overlayLayers() const noexcept;
    [[nodiscard]] bool logToTerminal() const noexcept;
    [[nodiscard]] bool logToFile() const noexcept;
    [[nodiscard]] QString logFilePath() const;
    [[nodiscard]] int ephemerisEngineKindIndex() const noexcept;
    [[nodiscard]] int ephemerisCorrectionPresetIndex() const noexcept;
    [[nodiscard]] bool ephemerisRefractionEnabled() const noexcept;
    [[nodiscard]] QString ephemerisAtmosphericPressureText() const;
    [[nodiscard]] QString ephemerisAtmosphericTemperatureText() const;
    [[nodiscard]] QString ephemerisRelativeHumidityText() const;
    [[nodiscard]] QString ephemerisWavelengthText() const;
    [[nodiscard]] QString locationStatusText() const;
    [[nodiscard]] QString catalogStatusText() const;
    [[nodiscard]] QString ephemerisDataStatusText() const;
    [[nodiscard]] QString ephemerisShortRangeKernelStatusText() const;
    [[nodiscard]] QString ephemerisLongRangeKernelStatusText() const;
    [[nodiscard]] QString ephemerisEarthOrientationStatusText() const;
    [[nodiscard]] QString ephemerisLeapSecondStatusText() const;
    [[nodiscard]] QString ephemerisDeltaTStatusText() const;
    [[nodiscard]] QString ephemerisDataLastUpdateResultText() const;
    [[nodiscard]] QString ephemerisPlanetaryKernelCacheSizeText() const;
    [[nodiscard]] QString ephemerisSupportDataCacheSizeText() const;
    [[nodiscard]] bool ephemerisDataOnlineUpdatesEnabled() const noexcept;
    [[nodiscard]] bool ephemerisDataUpdateEnabled() const noexcept;
    [[nodiscard]] bool ephemerisDataUpdateInProgress() const noexcept;
    [[nodiscard]] double ephemerisDataUpdateProgress() const noexcept;
    [[nodiscard]] QString catalogDatasetInfoText() const;
    [[nodiscard]] QString deepSkyCatalogInfoText() const;
    [[nodiscard]] QAbstractItemModel* objectSearchModel() const noexcept;
    [[nodiscard]] bool downloadingCatalog() const noexcept;
    [[nodiscard]] bool catalogProcessing() const noexcept;
    [[nodiscard]] QString selectedSearchTargetKind() const;
    [[nodiscard]] QString selectedSearchTargetId() const;
    [[nodiscard]] bool hasTrackedTarget() const;
    [[nodiscard]] QString trackedTargetKind() const;
    [[nodiscard]] QString trackedTargetId() const;
    [[nodiscard]] QString trackedTargetDisplayText() const;
    [[nodiscard]] QVariantMap nightConditions() const;
    [[nodiscard]] QString nightConditionsIconKind() const;
    [[nodiscard]] const skygate::core::SkyContext& skyContext() const noexcept;
    [[nodiscard]] std::uint64_t catalogRevision() const noexcept;
    [[nodiscard]] double viewFieldOfViewDeg() const noexcept;
    [[nodiscard]] skygate::core::ProjectionType projectionType() const noexcept;
    [[nodiscard]] const skygate::ui::internal::SkyThemeRenderPalette& renderTheme() const noexcept;
    [[nodiscard]] const SkyOverlayLayerVisibility& overlayLayerVisibility() const noexcept;
    [[nodiscard]] const skygate::ephemeris::IEphemerisEngine* ephemerisEngine() const noexcept;
    [[nodiscard]] std::shared_ptr<const skygate::ephemeris::IEphemerisDataSnapshot>
    activeEphemerisDataSnapshot() const noexcept;
    [[nodiscard]] std::uint64_t ephemerisDataRevision() const noexcept;
    [[nodiscard]] EphemerisRequestContext ephemerisRequestContext() const;
    [[nodiscard]] std::span<const skygate::ephemeris::CelestialBody> catalogBodies() const noexcept;
    [[nodiscard]] QStringList catalogSourceLabels() const;
    [[nodiscard]] std::span<const std::uint8_t> catalogSourceIds() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationLabelRef> constellationLabelRefs() const noexcept;

    Q_INVOKABLE void setLive(bool live);
    Q_INVOKABLE void setTimelineToolbarCollapsed(bool timelineToolbarCollapsed);
    Q_INVOKABLE void setSearchToolbarCollapsed(bool searchToolbarCollapsed);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void setSpeedMultiplier(double speedMultiplier);
    Q_INVOKABLE void setStepSeconds(int stepSeconds);
    Q_INVOKABLE void setMagnitudeCutoff(double magnitudeCutoff);
    Q_INVOKABLE void setViewCenter(double altitudeDeg, double azimuthDeg);
    Q_INVOKABLE void panViewBy(double deltaAzimuthDeg, double deltaAltitudeDeg);
    Q_INVOKABLE void zoomViewByWheelDelta(int wheelDeltaY);
    Q_INVOKABLE void zoomViewByScaleDelta(double scaleDelta);
    Q_INVOKABLE void goLiveNow();
    Q_INVOKABLE void resetViewDirection();
    Q_INVOKABLE void stepForward();
    Q_INVOKABLE void stepBackward();
    [[nodiscard]] QString validateUtcDateTimeText(const QString& utcDateText, const QString& utcTimeText) const;
    [[nodiscard]] bool setUtcDateTimeText(const QString& utcDateText, const QString& utcTimeText);
    Q_INVOKABLE void setLatitudeText(const QString& latitudeText);
    Q_INVOKABLE void setLongitudeText(const QString& longitudeText);
    Q_INVOKABLE void setElevationText(const QString& elevationText);
    Q_INVOKABLE void setLocationSourceText(const QString& locationSourceText);
    Q_INVOKABLE void setSelectedCityId(const QString& selectedCityId);
    Q_INVOKABLE void refreshCurrentLocation();
    Q_INVOKABLE void setProjectionTypeText(const QString& projectionTypeText);
    Q_INVOKABLE void setThemeId(const QString& themeId);
    Q_INVOKABLE void setLogToTerminal(bool logToTerminal);
    Q_INVOKABLE void setLogToFile(bool logToFile);
    Q_INVOKABLE void setLogFilePath(const QString& logFilePath);
    Q_INVOKABLE void setEphemerisEngineKindIndex(int engineKindIndex);
    Q_INVOKABLE void setEphemerisCorrectionPresetIndex(int correctionPresetIndex);
    Q_INVOKABLE void setEphemerisRefractionEnabled(bool enabled);
    Q_INVOKABLE void setEphemerisAtmosphericPressureText(const QString& pressureText);
    Q_INVOKABLE void setEphemerisAtmosphericTemperatureText(const QString& temperatureText);
    Q_INVOKABLE void setEphemerisRelativeHumidityText(const QString& humidityText);
    Q_INVOKABLE void setEphemerisWavelengthText(const QString& wavelengthText);
    Q_INVOKABLE bool saveSettings() const;
    Q_INVOKABLE bool loadSettings();
    Q_INVOKABLE bool clearCatalogCache();
    Q_INVOKABLE bool clearDeepSkyCatalogCache();
    Q_INVOKABLE bool clearEphemerisDataCache();
    Q_INVOKABLE bool clearPlanetaryKernelCache();
    Q_INVOKABLE bool clearSupportDataCache();
    Q_INVOKABLE bool updateEphemerisData();
    Q_INVOKABLE bool updateEphemerisDataProfile(const QString& profileId);
    Q_INVOKABLE bool checkEphemerisSupportDataUpdates();
    Q_INVOKABLE void cancelActiveDownload();
    Q_INVOKABLE void setEphemerisDataOnlineUpdatesEnabled(bool enabled);
    Q_INVOKABLE void loadCatalogPreset(const QString& presetId);
    Q_INVOKABLE void downloadCatalogFromUrl(const QString& urlText);
    Q_INVOKABLE void loadDeepSkyCatalogPreset(const QString& presetId);
    Q_INVOKABLE void downloadDeepSkyCatalogFromUrl(const QString& urlText);
    Q_INVOKABLE int catalogPresetIndex() const noexcept;
    Q_INVOKABLE void setCatalogPresetIndex(int catalogPresetIndex);
    Q_INVOKABLE QString catalogUrlText() const;
    Q_INVOKABLE void setCatalogUrlText(const QString& catalogUrlText);
    Q_INVOKABLE int deepSkyCatalogPresetIndex() const noexcept;
    Q_INVOKABLE void setDeepSkyCatalogPresetIndex(int deepSkyCatalogPresetIndex);
    Q_INVOKABLE QString deepSkyCatalogUrlText() const;
    Q_INVOKABLE void setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText);
    Q_INVOKABLE bool focusSearchTarget(const QString& targetKind, const QString& targetId);
    Q_INVOKABLE void clearSelectedSearchTarget();
    Q_INVOKABLE bool trackSearchTarget(const QString& targetKind, const QString& targetId);
    Q_INVOKABLE void clearTrackedTarget();
    Q_INVOKABLE void refreshNightConditions();

signals:
    void liveChanged();
    void timelineToolbarCollapsedChanged();
    void searchToolbarCollapsedChanged();
    void speedMultiplierChanged();
    void stepSecondsChanged();
    void magnitudeCutoffChanged();
    void viewDirectionChanged();
    void utcDateTextChanged();
    void utcTimeTextChanged();
    void latitudeTextChanged();
    void longitudeTextChanged();
    void elevationTextChanged();
    void locationSourceTextChanged();
    void selectedCityIdChanged();
    void selectedCityDisplayTextChanged();
    void projectionTypeChanged();
    void themeChanged();
    void themeOptionsChanged();
    void loggingChanged();
    void ephemerisSettingsChanged();
    void locationStatusTextChanged();
    void catalogStatusTextChanged();
    void ephemerisDataStatusTextChanged();
    void ephemerisDataChanged();
    void catalogDatasetInfoTextChanged();
    void deepSkyCatalogInfoTextChanged();
    void downloadingCatalogChanged();
    void catalogProcessingChanged();
    void selectedSearchTargetChanged();
    void trackedTargetChanged();
    void nightConditionsChanged();
    void skyContextChanged();

private:
    [[nodiscard]] QDateTime currentUtcDateTime() const;
    [[nodiscard]] skygate::core::UtcTimePoint currentUtcTime() const;
    [[nodiscard]] bool liveRecomputeThrottleApplies() const;
    [[nodiscard]] bool liveRecomputeThrottled(const skygate::core::UtcTimePoint& nextUtc) const;
    void markLiveRecomputeTick(const skygate::core::UtcTimePoint& nextUtc);
    void tickUtcTime();
    void stepBySeconds(int stepSeconds);
    void setCurrentUtc(const QDateTime& utcTime);
    void setCurrentUtcTime(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] bool setViewCenterInternal(double altitudeDeg, double azimuthDeg);
    [[nodiscard]] bool recenterTrackedTarget(bool emitSkyContextChangedWhenUnchanged = false);
    void setViewFieldOfViewDeg(double viewFieldOfViewDeg);
    void applyObserverLocation(const skygate::core::GeoLocation& observer);
    void initializeCurrentLocation();
    void applyCurrentLocation(const QGeoPositionInfo& positionInfo);
    void setLocationSource(skygate::ui::internal::SkyContextLocationSource locationSource);
    void clearSelectedCity();
    [[nodiscard]] bool applySelectedCityId(const QString& cityId);
    void setLocationStatusText(const QString& locationStatusText);
    void updateLocationStatusText();
    void setProjectionType(skygate::core::ProjectionType projectionType);
    void refreshObjectSearchModel();
    void applyLoggingConfiguration();
    void setSelectedSearchTarget(const QString& targetKind, const QString& targetId);
    void setTrackedTarget(const QString& targetKind, const QString& targetId, const QString& displayText);
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind activeEphemerisEngineKind() const noexcept;
    [[nodiscard]] EphemerisRequestContext ephemerisRequestContextFor(const skygate::core::SkyContext& skyContext) const;
    void applyEphemerisUserSettings(const SkySettingsStore::EphemerisUserSettingsSnapshot& settings);
    void setEphemerisDataOperationStatusText(QString statusText);
    void setEphemerisDataUpdateProgress(double progress) noexcept;
    [[nodiscard]] const skygate::ephemeris::EphemerisDataManifest* activeEphemerisDataManifest() const noexcept;
    [[nodiscard]] bool refreshEphemerisDataManifest(const QString& stagedRoot);
    void rebuildEphemerisEngine();

private:
    skygate::core::SystemTimeSource m_systemTimeSource;
    const skygate::core::ITimeSource* m_timeSource = nullptr;
    skygate::ui::internal::SkyTimelineController m_timeline;
    skygate::ui::internal::SkyViewController m_view;
    skygate::ui::internal::SkyLocationController m_location;
    skygate::ui::internal::SkySearchController m_search;
    QTimer m_timer;
    std::optional<skygate::core::UtcTimePoint> m_lastLiveRecomputeUtc;
    SkyLiveClock m_liveClock;
    std::unique_ptr<LocationCatalogModel> m_locationCatalogModel;
    std::unique_ptr<skygate::ui::internal::SkyThemePalette> m_themePalette;
    std::unique_ptr<skygate::ui::internal::SkyThemeRepository> m_themeRepository;
    std::unique_ptr<SkyTimeController> m_timeController;
    std::unique_ptr<SkyOverlayLayerSettings> m_overlayLayerSettings;
    std::unique_ptr<SkySettingsStore> m_settingsStore;
    std::unique_ptr<SkyEphemerisDataManager> m_ephemerisDataManager;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> m_ephemerisEngine;
    skygate::ephemeris::EphemerisEngineKind m_ephemerisEngineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    skygate::ephemeris::EphemerisEngineOptions m_ephemerisEngineOptions;
    std::uint64_t m_ephemerisOptionsRevision = 1U;
    SkySettingsStore::EphemerisUserSettingsSnapshot m_ephemerisUserSettings;
    const skygate::ephemeris::EphemerisDataSetInfo* m_ephemerisDataSetManifest = nullptr;
    const skygate::ephemeris::EphemerisDataManifest* m_ephemerisDataManifest = nullptr;
    std::optional<skygate::ephemeris::EphemerisDataManifest> m_refreshedEphemerisDataManifest;
    QString m_ephemerisUpdateResourceRoot;
    QString m_ephemerisWritableCacheRoot;
    QString m_ephemerisDataOperationStatusText;
    QString m_availableEarthOrientationVersion;
    QString m_availableLeapSecondVersion;
    QString m_availableDeltaTVersion;
    bool m_ephemerisSupportDataUpdateChecked = false;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_ephemerisTimeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> m_ephemerisEarthOrientationProvider;
    std::shared_ptr<const skygate::ephemeris::highprecision::ICalcephKernelRuntime> m_ephemerisCalcephKernelRuntime;
    skygate::ephemeris::IEphemerisDiagnosticsSink* m_ephemerisDiagnosticsSink = nullptr;
    std::unique_ptr<SkyCatalogManager> m_catalogManager;
    std::unique_ptr<SkyObjectSearchModel> m_objectSearchModel;
    QVariantList m_themeOptions;
    bool m_logToTerminal = true;
    bool m_logToFile = false;
    QString m_logFilePath;
    QVariantMap m_nightConditions;
    bool m_ephemerisDataUpdateInProgress = false;
    double m_ephemerisDataUpdateProgress = 0.0;
};
