#pragma once

#include <QObject>
#include <QColor>
#include <QDateTime>
#include <QStringList>
#include <QTimer>

#include "skygate/core/IProjection.hpp"
#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <vector>

class QGeoPositionInfo;
class QGeoPositionInfoSource;
class QNetworkAccessManager;
class CatalogCoordinator;

class SkyContextController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
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
    Q_PROPERTY(QString projectionTypeText READ projectionTypeText NOTIFY projectionTypeChanged)
    Q_PROPERTY(QString projectionSampleText READ projectionSampleText NOTIFY projectionTypeChanged)
    Q_PROPERTY(QString locationStatusText READ locationStatusText NOTIFY locationStatusTextChanged)
    Q_PROPERTY(QString catalogStatusText READ catalogStatusText NOTIFY catalogStatusTextChanged)
    Q_PROPERTY(bool downloadingCatalog READ downloadingCatalog NOTIFY downloadingCatalogChanged)
    Q_PROPERTY(QString skyContextSummary READ skyContextSummary NOTIFY skyContextChanged)

public:
    struct SkyRenderPoint {
        double x = 0.0;
        double y = 0.0;
        double sizePx = 2.0;
        QString displayName;
        QColor color;
    };

    struct SkyRenderLine {
        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
        QColor color;
    };

public:
    explicit SkyContextController(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog = nullptr,
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine = nullptr,
        QObject* parent = nullptr
    );
    ~SkyContextController() override;

    [[nodiscard]] bool live() const noexcept;
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
    [[nodiscard]] QString projectionTypeText() const;
    [[nodiscard]] QString projectionSampleText() const;
    [[nodiscard]] QString locationStatusText() const;
    [[nodiscard]] QString catalogStatusText() const;
    [[nodiscard]] bool downloadingCatalog() const noexcept;
    [[nodiscard]] QString skyContextSummary() const;
    [[nodiscard]] const skygate::core::SkyContext& skyContext() const noexcept;
    [[nodiscard]] std::vector<SkyRenderPoint> renderPoints(
        double viewportWidth,
        double viewportHeight
    ) const;
    [[nodiscard]] std::vector<SkyRenderLine> renderConstellationLines(
        double viewportWidth,
        double viewportHeight
    ) const;
    [[nodiscard]] skygate::core::ScreenPoint projectHorizontal(
        const skygate::core::HorizontalCoordinate& coordinate,
        double viewportWidth,
        double viewportHeight
    ) const noexcept;

    Q_INVOKABLE void setLive(bool live);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void setSpeedMultiplier(double speedMultiplier);
    Q_INVOKABLE void setStepSeconds(int stepSeconds);
    Q_INVOKABLE void setMagnitudeCutoff(double magnitudeCutoff);
    Q_INVOKABLE void setViewCenter(double altitudeDeg, double azimuthDeg);
    Q_INVOKABLE void panViewBy(double deltaAzimuthDeg, double deltaAltitudeDeg);
    Q_INVOKABLE void zoomViewByWheelDelta(int wheelDeltaY);
    Q_INVOKABLE void resetViewDirection();
    Q_INVOKABLE void stepForward();
    Q_INVOKABLE void stepBackward();
    Q_INVOKABLE void setUtcDateText(const QString& utcDateText);
    Q_INVOKABLE void setUtcTimeText(const QString& utcTimeText);
    Q_INVOKABLE void setLatitudeText(const QString& latitudeText);
    Q_INVOKABLE void setLongitudeText(const QString& longitudeText);
    Q_INVOKABLE void setElevationText(const QString& elevationText);
    Q_INVOKABLE void setProjectionTypeText(const QString& projectionTypeText);
    Q_INVOKABLE bool saveSettings() const;
    Q_INVOKABLE bool loadSettings();
    Q_INVOKABLE double projectedX(double altitudeDeg, double azimuthDeg, double viewportWidth, double viewportHeight) const;
    Q_INVOKABLE double projectedY(double altitudeDeg, double azimuthDeg, double viewportWidth, double viewportHeight) const;
    Q_INVOKABLE bool isProjectedVisible(double altitudeDeg, double azimuthDeg, double viewportWidth, double viewportHeight) const;
    Q_INVOKABLE QString objectLabelAt(double x, double y, double viewportWidth, double viewportHeight) const;
    Q_INVOKABLE void loadCatalogPreset(const QString& presetId);
    Q_INVOKABLE void downloadCatalogFromUrl(const QString& urlText);
    Q_INVOKABLE int catalogPresetIndex() const noexcept;
    Q_INVOKABLE void setCatalogPresetIndex(int catalogPresetIndex);
    Q_INVOKABLE QString catalogUrlText() const;
    Q_INVOKABLE void setCatalogUrlText(const QString& catalogUrlText);

signals:
    void liveChanged();
    void speedMultiplierChanged();
    void stepSecondsChanged();
    void magnitudeCutoffChanged();
    void viewDirectionChanged();
    void utcDateTextChanged();
    void utcTimeTextChanged();
    void latitudeTextChanged();
    void longitudeTextChanged();
    void elevationTextChanged();
    void invalidUtcDateInput(const QString& utcDateText);
    void invalidUtcTimeInput(const QString& utcTimeText);
    void invalidLatitudeInput(const QString& latitudeText);
    void invalidLongitudeInput(const QString& longitudeText);
    void invalidElevationInput(const QString& elevationText);
    void projectionTypeChanged();
    void locationStatusTextChanged();
    void catalogStatusTextChanged();
    void downloadingCatalogChanged();
    void skyContextChanged();

private:
    void tickUtcTime();
    void stepBySeconds(int stepSeconds);
    void setCurrentUtc(const QDateTime& utcTime);
    void setViewFieldOfViewDeg(double viewFieldOfViewDeg);
    void initializeCurrentLocation();
    void applyCurrentLocation(const QGeoPositionInfo& positionInfo);
    void setProjectionType(skygate::core::ProjectionType projectionType);
    void applyCatalog(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
        const QString& sourceLabel,
        bool persistCatalog = true
    );
    void downloadCatalogFromUrls(const QStringList& urlTexts, const QString& sourceLabel);
    void persistCatalogCache(
        const std::vector<skygate::ephemeris::CelestialBody>& bodies,
        const QString& sourceLabel
    ) const;
    void restoreCatalogCache();

private:
    bool m_live = true;
    double m_speedMultiplier = 1.0;
    double m_speedRemainderSeconds = 0.0;
    int m_stepSeconds = 60;
    double m_magnitudeCutoff = 6.0;
    double m_viewCenterAltitudeDeg = 45.0;
    double m_viewCenterAzimuthDeg = 180.0;
    double m_viewFieldOfViewDeg = 100.0;
    QTimer m_timer;
    skygate::core::SkyContext m_skyContext;
    skygate::core::ProjectionType m_projectionType = skygate::core::ProjectionType::Stereographic;
    std::unique_ptr<skygate::core::IProjection> m_projection;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> m_ephemerisEngine;
    std::unique_ptr<CatalogCoordinator> m_catalogCoordinator;
    QNetworkAccessManager* m_networkAccessManager = nullptr;
    QString m_locationStatusText;
    QString m_catalogStatusText;
    QString m_catalogSourceLabel = "Bundled";
    int m_catalogPresetIndex = 0;
    QString m_catalogUrlText;
    bool m_downloadingCatalog = false;
    QGeoPositionInfoSource* m_positionSource = nullptr;
};
