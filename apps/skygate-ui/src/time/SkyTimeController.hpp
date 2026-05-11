#pragma once

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QTimeZone>

#include "skygate/core/Types.hpp"

#include <optional>

class QAbstractItemModel;
class TimeZoneCatalogModel;

namespace skygate::core {
class ITimeSource;
}

class SkyTimeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString dateText READ dateText NOTIFY dateTextChanged)
    Q_PROPERTY(QString timeText READ timeText NOTIFY timeTextChanged)
    Q_PROPERTY(QString timeZoneId READ timeZoneId NOTIFY timeZoneChanged)
    Q_PROPERTY(QString timeZoneLabel READ timeZoneLabel NOTIFY timeZoneChanged)
    Q_PROPERTY(QString timeZoneDetailText READ timeZoneDetailText NOTIFY timeZoneChanged)
    Q_PROPERTY(QAbstractItemModel* timeZoneCatalogModel READ timeZoneCatalogModel CONSTANT)

public:
    explicit SkyTimeController(QObject* parent = nullptr);
    explicit SkyTimeController(
        const skygate::core::ITimeSource& timeSource,
        QObject* parent = nullptr
    );
    ~SkyTimeController() override;

    [[nodiscard]] QString dateText() const;
    [[nodiscard]] QString timeText() const;
    [[nodiscard]] QString timeZoneId() const;
    [[nodiscard]] QString timeZoneLabel() const;
    [[nodiscard]] QString timeZoneDetailText() const;
    [[nodiscard]] QAbstractItemModel* timeZoneCatalogModel() const noexcept;
    [[nodiscard]] QDateTime utcDateTime() const;
    [[nodiscard]] skygate::core::UtcTimePoint utcTimePoint() const;

    void setUtcDateTime(const QDateTime& utcDateTime);
    void setUtcTimePoint(const skygate::core::UtcTimePoint& utcTime);
    bool setInitialTimeZoneId(const QString& timeZoneId);

    Q_INVOKABLE QString validateDateTimeText(
        const QString& dateText,
        const QString& timeText
    ) const;
    Q_INVOKABLE bool setDateTimeText(const QString& dateText, const QString& timeText);
    Q_INVOKABLE bool setTimeZoneId(const QString& timeZoneId);
    Q_INVOKABLE void goLiveNow();
    [[nodiscard]] QString formatUtcTime(
        const skygate::core::UtcTimePoint& utcTime,
        bool includeSeconds = true
    ) const;
    [[nodiscard]] QString formatUtcTimeRange(
        const skygate::core::UtcTimePoint& startUtc,
        const skygate::core::UtcTimePoint& endUtc
    ) const;

signals:
    void dateTextChanged();
    void timeTextChanged();
    void timeZoneChanged();
    void utcDateTimeChangeRequested(const QDateTime& utcDateTime);
    void goLiveNowRequested();

private:
    struct ParseResult final {
        QDateTime utcDateTime;
        QString errorText;

        [[nodiscard]] bool isValid() const noexcept;
    };

private:
    [[nodiscard]] QDateTime displayDateTime() const;
    [[nodiscard]] ParseResult parseDisplayDateTime(
        const QString& dateText,
        const QString& timeText
    ) const;
    [[nodiscard]] QString formattedDateText() const;
    [[nodiscard]] QString formattedTimeText() const;
    void emitDisplayTextChanges(const QString& previousDateText, const QString& previousTimeText);

private:
    QDateTime m_utcDateTime;
    QTimeZone m_timeZone;
    TimeZoneCatalogModel* m_timeZoneCatalogModel = nullptr;
};
