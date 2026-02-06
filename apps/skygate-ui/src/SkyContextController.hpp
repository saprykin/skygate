#pragma once

#include <QObject>
#include <QDateTime>
#include <QTimer>

#include "skygate/core/Types.hpp"

class QGeoPositionInfo;
class QGeoPositionInfoSource;

class SkyContextController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(QString utcDateText READ utcDateText NOTIFY utcDateTextChanged)
    Q_PROPERTY(QString utcTimeText READ utcTimeText NOTIFY utcTimeTextChanged)
    Q_PROPERTY(QString latitudeText READ latitudeText NOTIFY latitudeTextChanged)
    Q_PROPERTY(QString longitudeText READ longitudeText NOTIFY longitudeTextChanged)
    Q_PROPERTY(QString elevationText READ elevationText NOTIFY elevationTextChanged)
    Q_PROPERTY(QString locationStatusText READ locationStatusText NOTIFY locationStatusTextChanged)
    Q_PROPERTY(QString skyContextSummary READ skyContextSummary NOTIFY skyContextChanged)

public:
    explicit SkyContextController(QObject* parent = nullptr);

    [[nodiscard]] bool live() const noexcept;
    [[nodiscard]] QString utcDateText() const;
    [[nodiscard]] QString utcTimeText() const;
    [[nodiscard]] QString latitudeText() const;
    [[nodiscard]] QString longitudeText() const;
    [[nodiscard]] QString elevationText() const;
    [[nodiscard]] QString locationStatusText() const;
    [[nodiscard]] QString skyContextSummary() const;
    [[nodiscard]] const skygate::core::SkyContext& skyContext() const noexcept;

    Q_INVOKABLE void setLive(bool live);
    Q_INVOKABLE void setUtcDateText(const QString& utcDateText);
    Q_INVOKABLE void setUtcTimeText(const QString& utcTimeText);
    Q_INVOKABLE void setLatitudeText(const QString& latitudeText);
    Q_INVOKABLE void setLongitudeText(const QString& longitudeText);
    Q_INVOKABLE void setElevationText(const QString& elevationText);

signals:
    void liveChanged();
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
    void locationStatusTextChanged();
    void skyContextChanged();

private:
    void tickUtcTime();
    void setCurrentUtc(const QDateTime& utcTime);
    void initializeCurrentLocation();
    void applyCurrentLocation(const QGeoPositionInfo& positionInfo);

private:
    bool m_live = true;
    QTimer m_timer;
    skygate::core::SkyContext m_skyContext;
    QString m_locationStatusText;
    QGeoPositionInfoSource* m_positionSource = nullptr;
};
