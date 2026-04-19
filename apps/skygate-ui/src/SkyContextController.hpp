#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "skygate/core/IProjection.hpp"
#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include "SkyContextControllerSupport.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

class QDateTime;
class QAbstractItemModel;
class QGeoPositionInfo;
class QGeoPositionInfoSource;
class LocationCatalogModel;
class SkyCatalogManager;
class SkySettingsStore;

class SkyContextController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(bool utcDateLocked READ utcDateLocked WRITE setUtcDateLocked NOTIFY utcDateLockedChanged)
    Q_PROPERTY(bool utcTimeLocked READ utcTimeLocked WRITE setUtcTimeLocked NOTIFY utcTimeLockedChanged)
    Q_PROPERTY(
        bool timelineToolbarCollapsed
        READ timelineToolbarCollapsed
        WRITE setTimelineToolbarCollapsed
        NOTIFY timelineToolbarCollapsedChanged
    )
    Q_PROPERTY(double speedMultiplier READ speedMultiplier WRITE setSpeedMultiplier NOTIFY speedMultiplierChanged)
    Q_PROPERTY(int stepSeconds READ stepSeconds WRITE setStepSeconds NOTIFY stepSecondsChanged)
    Q_PROPERTY(double magnitudeCutoff READ magnitudeCutoff WRITE setMagnitudeCutoff NOTIFY magnitudeCutoffChanged)
    Q_PROPERTY(double viewCenterAltitudeDeg READ viewCenterAltitudeDeg NOTIFY viewDirectionChanged)
    Q_PROPERTY(double viewCenterAzimuthDeg READ viewCenterAzimuthDeg NOTIFY viewDirectionChanged)
    Q_PROPERTY(QString utcDateText READ utcDateText NOTIFY utcDateTextChanged)
    Q_PROPERTY(QString utcTimeText READ utcTimeText NOTIFY utcTimeTextChanged)
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
    Q_PROPERTY(QString projectionSampleText READ projectionSampleText NOTIFY projectionTypeChanged)
    Q_PROPERTY(QString locationStatusText READ locationStatusText NOTIFY locationStatusTextChanged)
    Q_PROPERTY(QString catalogStatusText READ catalogStatusText NOTIFY catalogStatusTextChanged)
    Q_PROPERTY(
        QString catalogDatasetInfoText
        READ catalogDatasetInfoText
        NOTIFY catalogDatasetInfoTextChanged
    )
    Q_PROPERTY(bool downloadingCatalog READ downloadingCatalog NOTIFY downloadingCatalogChanged)
    Q_PROPERTY(bool catalogProcessing READ catalogProcessing NOTIFY catalogProcessingChanged)
    Q_PROPERTY(QString skyContextSummary READ skyContextSummary NOTIFY skyContextChanged)

public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationLabelRef = skygate::ephemeris::ConstellationLabelRef;

public:
    struct InitializationOptions final {
        bool loadSettings;
        bool initializeLocation;
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
    [[nodiscard]] bool utcDateLocked() const noexcept;
    [[nodiscard]] bool utcTimeLocked() const noexcept;
    [[nodiscard]] bool timelineToolbarCollapsed() const noexcept;
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
    [[nodiscard]] QString projectionSampleText() const;
    [[nodiscard]] QString locationStatusText() const;
    [[nodiscard]] QString catalogStatusText() const;
    [[nodiscard]] QString catalogDatasetInfoText() const;
    [[nodiscard]] bool downloadingCatalog() const noexcept;
    [[nodiscard]] bool catalogProcessing() const noexcept;
    [[nodiscard]] QString skyContextSummary() const;
    [[nodiscard]] const skygate::core::SkyContext& skyContext() const noexcept;
    [[nodiscard]] std::uint64_t catalogRevision() const noexcept;
    [[nodiscard]] double viewFieldOfViewDeg() const noexcept;
    [[nodiscard]] skygate::core::ProjectionType projectionType() const noexcept;
    [[nodiscard]] const skygate::ephemeris::IEphemerisEngine* ephemerisEngine() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationLabelRef> constellationLabelRefs() const noexcept;

    Q_INVOKABLE void setLive(bool live);
    Q_INVOKABLE void setUtcDateLocked(bool utcDateLocked);
    Q_INVOKABLE void setUtcTimeLocked(bool utcTimeLocked);
    Q_INVOKABLE void setTimelineToolbarCollapsed(bool timelineToolbarCollapsed);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void setSpeedMultiplier(double speedMultiplier);
    Q_INVOKABLE void setStepSeconds(int stepSeconds);
    Q_INVOKABLE void setMagnitudeCutoff(double magnitudeCutoff);
    Q_INVOKABLE void setViewCenter(double altitudeDeg, double azimuthDeg);
    Q_INVOKABLE void panViewBy(double deltaAzimuthDeg, double deltaAltitudeDeg);
    Q_INVOKABLE void zoomViewByWheelDelta(int wheelDeltaY);
    Q_INVOKABLE void zoomViewByScaleDelta(double scaleDelta);
    Q_INVOKABLE void resetViewDirection();
    Q_INVOKABLE void stepForward();
    Q_INVOKABLE void stepBackward();
    Q_INVOKABLE void setUtcDateText(const QString& utcDateText);
    Q_INVOKABLE void setUtcTimeText(const QString& utcTimeText);
    Q_INVOKABLE void setLatitudeText(const QString& latitudeText);
    Q_INVOKABLE void setLongitudeText(const QString& longitudeText);
    Q_INVOKABLE void setElevationText(const QString& elevationText);
    Q_INVOKABLE void setLocationSourceText(const QString& locationSourceText);
    Q_INVOKABLE void setSelectedCityId(const QString& selectedCityId);
    Q_INVOKABLE void refreshCurrentLocation();
    Q_INVOKABLE void setProjectionTypeText(const QString& projectionTypeText);
    Q_INVOKABLE bool saveSettings() const;
    Q_INVOKABLE bool loadSettings();
    Q_INVOKABLE bool clearCatalogCache();
    Q_INVOKABLE void loadCatalogPreset(const QString& presetId);
    Q_INVOKABLE void downloadCatalogFromUrl(const QString& urlText);
    Q_INVOKABLE int catalogPresetIndex() const noexcept;
    Q_INVOKABLE void setCatalogPresetIndex(int catalogPresetIndex);
    Q_INVOKABLE QString catalogUrlText() const;
    Q_INVOKABLE void setCatalogUrlText(const QString& catalogUrlText);

signals:
    void liveChanged();
    void utcDateLockedChanged();
    void utcTimeLockedChanged();
    void timelineToolbarCollapsedChanged();
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
    void invalidUtcDateInput(const QString& utcDateText);
    void invalidUtcTimeInput(const QString& utcTimeText);
    void invalidLatitudeInput(const QString& latitudeText);
    void invalidLongitudeInput(const QString& longitudeText);
    void invalidElevationInput(const QString& elevationText);
    void projectionTypeChanged();
    void locationStatusTextChanged();
    void catalogStatusTextChanged();
    void catalogDatasetInfoTextChanged();
    void downloadingCatalogChanged();
    void catalogProcessingChanged();
    void skyContextChanged();

private:
    void tickUtcTime();
    void stepBySeconds(int stepSeconds);
    void setCurrentUtc(const QDateTime& utcTime);
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

private:
    bool m_live = true;
    bool m_utcDateLocked = true;
    bool m_utcTimeLocked = true;
    bool m_timelineToolbarCollapsed = false;
    bool m_restoreUtcLockStateOnLiveResume = false;
    bool m_restoreUtcDateLockedOnLiveResume = false;
    bool m_restoreUtcTimeLockedOnLiveResume = false;
    double m_speedMultiplier = 1.0;
    double m_speedRemainderSeconds = 0.0;
    int m_stepSeconds = 60;
    double m_magnitudeCutoff = 6.0;
    double m_viewCenterAltitudeDeg = skygate::core::ViewportMath::kDefaultCenterAltitudeDeg;
    double m_viewCenterAzimuthDeg = skygate::core::ViewportMath::kDefaultCenterAzimuthDeg;
    double m_viewFieldOfViewDeg = 100.0;
    QTimer m_timer;
    skygate::core::SkyContext m_skyContext;
    skygate::ui::internal::SkyContextLocationSource m_locationSource =
        skygate::ui::internal::SkyContextLocationSourceCodec::defaultSource();
    skygate::core::ProjectionType m_projectionType = skygate::core::ProjectionType::Stereographic;
    std::unique_ptr<skygate::core::IProjection> m_projection;
    std::unique_ptr<LocationCatalogModel> m_locationCatalogModel;
    std::unique_ptr<SkySettingsStore> m_settingsStore;
    std::unique_ptr<SkyCatalogManager> m_catalogManager;
    QString m_locationStatusText;
    QString m_selectedCityId;
    QString m_selectedCityDisplayText;
    QGeoPositionInfoSource* m_positionSource = nullptr;
    bool m_positionSourceConnected = false;
};
