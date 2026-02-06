#pragma once

#include <QObject>
#include <QDateTime>
#include <QTimer>

class SkyContextController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(QString utcDateText READ utcDateText NOTIFY utcDateTextChanged)
    Q_PROPERTY(QString utcTimeText READ utcTimeText NOTIFY utcTimeTextChanged)

public:
    explicit SkyContextController(QObject* parent = nullptr);

    [[nodiscard]] bool live() const noexcept;
    [[nodiscard]] QString utcDateText() const;
    [[nodiscard]] QString utcTimeText() const;

    Q_INVOKABLE void setLive(bool live);
    Q_INVOKABLE void setUtcDateText(const QString& utcDateText);
    Q_INVOKABLE void setUtcTimeText(const QString& utcTimeText);

signals:
    void liveChanged();
    void utcDateTextChanged();
    void utcTimeTextChanged();
    void invalidUtcDateInput(const QString& utcDateText);
    void invalidUtcTimeInput(const QString& utcTimeText);

private:
    void tickUtcTime();
    void setCurrentUtc(const QDateTime& utcTime);

private:
    bool m_live = true;
    QDateTime m_currentUtc;
    QTimer m_timer;
};
