#include "SkyContextController.hpp"

#include <Qt>
#include <QTimeZone>

namespace {
constexpr auto kTickIntervalMs = 1000;
}

SkyContextController::SkyContextController(QObject* parent)
    : QObject(parent)
{
    m_currentUtc = QDateTime::currentDateTimeUtc().toUTC();

    m_timer.setInterval(kTickIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SkyContextController::tickUtcTime);
    m_timer.start();
}

bool SkyContextController::live() const noexcept
{
    return m_live;
}

QString SkyContextController::utcTimeText() const
{
    return m_currentUtc.toString("HH:mm:ss");
}

QString SkyContextController::utcDateText() const
{
    return m_currentUtc.toString("yyyy-MM-dd");
}

void SkyContextController::setLive(bool live)
{
    if (m_live == live) {
        return;
    }

    m_live = live;
    emit liveChanged();
}

void SkyContextController::setUtcDateText(const QString& utcDateText)
{
    const auto date = QDate::fromString(utcDateText, "yyyy-MM-dd");
    if (!date.isValid()) {
        emit invalidUtcDateInput(utcDateText);
        emit utcDateTextChanged();
        return;
    }

    const QDateTime nextUtc(
        date,
        m_currentUtc.time(),
        QTimeZone::UTC
    );

    setCurrentUtc(nextUtc);
    setLive(false);
}

void SkyContextController::setUtcTimeText(const QString& utcTimeText)
{
    const auto time = QTime::fromString(utcTimeText, "HH:mm:ss");
    if (!time.isValid()) {
        emit invalidUtcTimeInput(utcTimeText);
        emit utcTimeTextChanged();
        return;
    }

    const QDateTime nextUtc(
        m_currentUtc.date(),
        time,
        QTimeZone::UTC
    );

    setCurrentUtc(nextUtc);
    setLive(false);
}

void SkyContextController::tickUtcTime()
{
    if (!m_live) {
        return;
    }

    setCurrentUtc(m_currentUtc.addSecs(1));
}

void SkyContextController::setCurrentUtc(const QDateTime& utcTime)
{
    if (m_currentUtc == utcTime) {
        return;
    }

    m_currentUtc = utcTime;
    emit utcDateTextChanged();
    emit utcTimeTextChanged();
}
