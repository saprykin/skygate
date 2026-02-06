#pragma once

#include <QObject>
#include <QColor>
#include <QDateTime>
#include <QTimer>

#include "skygate/core/IProjection.hpp"
#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <memory>
#include <vector>

class QGeoPositionInfo;
class QGeoPositionInfoSource;

class SkyContextController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(double speedMultiplier READ speedMultiplier WRITE setSpeedMultiplier NOTIFY speedMultiplierChanged)
    Q_PROPERTY(int stepSeconds READ stepSeconds WRITE setStepSeconds NOTIFY stepSecondsChanged)
    Q_PROPERTY(QString utcDateText READ utcDateText NOTIFY utcDateTextChanged)
    Q_PROPERTY(QString utcTimeText READ utcTimeText NOTIFY utcTimeTextChanged)
    Q_PROPERTY(QString latitudeText READ latitudeText NOTIFY latitudeTextChanged)
    Q_PROPERTY(QString longitudeText READ longitudeText NOTIFY longitudeTextChanged)
    Q_PROPERTY(QString elevationText READ elevationText NOTIFY elevationTextChanged)
    Q_PROPERTY(QString projectionTypeText READ projectionTypeText NOTIFY projectionTypeChanged)
    Q_PROPERTY(QString projectionSampleText READ projectionSampleText NOTIFY projectionTypeChanged)
    Q_PROPERTY(QString locationStatusText READ locationStatusText NOTIFY locationStatusTextChanged)
    Q_PROPERTY(QString skyContextSummary READ skyContextSummary NOTIFY skyContextChanged)

public:
    struct SkyRenderPoint {
        double x = 0.0;
        double y = 0.0;
        double sizePx = 2.0;
        QColor color;
    };

public:
    explicit SkyContextController(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog = nullptr,
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine = nullptr,
        QObject* parent = nullptr
    );

    [[nodiscard]] bool live() const noexcept;
    [[nodiscard]] double speedMultiplier() const noexcept;
    [[nodiscard]] int stepSeconds() const noexcept;
    [[nodiscard]] QString utcDateText() const;
    [[nodiscard]] QString utcTimeText() const;
    [[nodiscard]] QString latitudeText() const;
    [[nodiscard]] QString longitudeText() const;
    [[nodiscard]] QString elevationText() const;
    [[nodiscard]] QString projectionTypeText() const;
    [[nodiscard]] QString projectionSampleText() const;
    [[nodiscard]] QString locationStatusText() const;
    [[nodiscard]] QString skyContextSummary() const;
    [[nodiscard]] const skygate::core::SkyContext& skyContext() const noexcept;
    [[nodiscard]] std::vector<SkyRenderPoint> renderPoints(
        double viewportWidth,
        double viewportHeight
    ) const;

    Q_INVOKABLE void setLive(bool live);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void setSpeedMultiplier(double speedMultiplier);
    Q_INVOKABLE void setStepSeconds(int stepSeconds);
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

signals:
    void liveChanged();
    void speedMultiplierChanged();
    void stepSecondsChanged();
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
    void skyContextChanged();

private:
    void tickUtcTime();
    void stepBySeconds(int stepSeconds);
    void setCurrentUtc(const QDateTime& utcTime);
    void initializeCurrentLocation();
    void applyCurrentLocation(const QGeoPositionInfo& positionInfo);
    void setProjectionType(skygate::core::ProjectionType projectionType);

private:
    bool m_live = true;
    double m_speedMultiplier = 1.0;
    double m_speedRemainderSeconds = 0.0;
    int m_stepSeconds = 60;
    QTimer m_timer;
    skygate::core::SkyContext m_skyContext;
    skygate::core::ProjectionType m_projectionType = skygate::core::ProjectionType::Stereographic;
    std::unique_ptr<skygate::core::IProjection> m_projection;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> m_ephemerisEngine;
    QString m_locationStatusText;
    QGeoPositionInfoSource* m_positionSource = nullptr;
};
