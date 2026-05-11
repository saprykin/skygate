#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include "skygate/core/Types.hpp"
#include "skygate/core/SystemTimeSource.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyContextDomainControllers.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyTimeController.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

class QDateTime;
class QGeoPositionInfo;
class QGeoPositionInfoSource;
class LocationCatalogModel;
class SkyCatalogManager;
class SkyObjectSearchModel;
class SkyOverlayLayerSettings;
class SkySettingsStore;

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
    Q_PROPERTY(QString locationStatusText READ locationStatusText NOTIFY locationStatusTextChanged)
    Q_PROPERTY(QString catalogStatusText READ catalogStatusText NOTIFY catalogStatusTextChanged)
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
    struct InitializationOptions final {
        bool loadSettings = true;
        bool initializeLocation = true;
        QGeoPositionInfoSource* positionSource = nullptr;
        bool requestLocationPermission = true;
        const skygate::core::ITimeSource* timeSource = nullptr;
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
    [[nodiscard]] QString locationStatusText() const;
    [[nodiscard]] QString catalogStatusText() const;
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
    [[nodiscard]] QString validateUtcDateTimeText(
        const QString& utcDateText,
        const QString& utcTimeText
    ) const;
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
    Q_INVOKABLE bool saveSettings() const;
    Q_INVOKABLE bool loadSettings();
    Q_INVOKABLE bool clearCatalogCache();
    Q_INVOKABLE bool clearDeepSkyCatalogCache();
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
    void locationStatusTextChanged();
    void catalogStatusTextChanged();
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
    void tickUtcTime();
    void stepBySeconds(int stepSeconds);
    void setCurrentUtc(const QDateTime& utcTime);
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
    void setTrackedTarget(
        const QString& targetKind,
        const QString& targetId,
        const QString& displayText
    );

private:
    skygate::core::SystemTimeSource m_systemTimeSource;
    const skygate::core::ITimeSource* m_timeSource = nullptr;
    skygate::ui::internal::SkyTimelineController m_timeline;
    skygate::ui::internal::SkyViewController m_view;
    skygate::ui::internal::SkyLocationController m_location;
    skygate::ui::internal::SkySearchController m_search;
    QTimer m_timer;
    std::unique_ptr<LocationCatalogModel> m_locationCatalogModel;
    std::unique_ptr<skygate::ui::internal::SkyThemePalette> m_themePalette;
    std::unique_ptr<skygate::ui::internal::SkyThemeRepository> m_themeRepository;
    std::unique_ptr<SkyTimeController> m_timeController;
    std::unique_ptr<SkyOverlayLayerSettings> m_overlayLayerSettings;
    std::unique_ptr<SkySettingsStore> m_settingsStore;
    std::unique_ptr<SkyCatalogManager> m_catalogManager;
    std::unique_ptr<SkyObjectSearchModel> m_objectSearchModel;
    QVariantList m_themeOptions;
    bool m_logToTerminal = true;
    bool m_logToFile = false;
    QString m_logFilePath;
    QVariantMap m_nightConditions;
};
