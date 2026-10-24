#include "SkyTimeController.hpp"

#include "SkyQtTimeCodec.hpp"
#include "TimeZoneCatalogModel.hpp"

#include "ITimeSource.hpp"
#include "SystemTimeSource.hpp"
#include "UtcTimeCodec.hpp"

#include <QTimeZone>

#include <chrono>
#include <optional>

namespace {

// Single validation path for persisted and user supplied time zone ids.
std::optional<QTimeZone> availableTimeZone(const QString& timeZoneId)
{
    const QByteArray idBytes = timeZoneId.trimmed().toUtf8();
    if (idBytes.isEmpty() || !QTimeZone::isTimeZoneIdAvailable(idBytes)) {
        return std::nullopt;
    }

    return QTimeZone(idBytes);
}

QTimeZone systemTimeZoneOrUtc()
{
    const std::optional<QTimeZone> systemTimeZone = availableTimeZone(QString::fromUtf8(QTimeZone::systemTimeZoneId()));
    return systemTimeZone.has_value() ? *systemTimeZone : QTimeZone::utc();
}

// Startup fallback: a missing id follows the system zone, while an id that is
// present but unavailable falls back to UTC.
QTimeZone initialTimeZoneOrFallback(const QString& timeZoneId)
{
    const std::optional<QTimeZone> timeZone = availableTimeZone(timeZoneId);
    if (timeZone.has_value()) {
        return *timeZone;
    }

    return timeZoneId.trimmed().isEmpty() ? systemTimeZoneOrUtc() : QTimeZone::utc();
}

const skygate::core::SystemTimeSource& defaultTimeSource()
{
    static const skygate::core::SystemTimeSource timeSource;
    return timeSource;
}

}  // namespace

SkyTimeController::SkyTimeController(QObject* parent) : SkyTimeController(defaultTimeSource(), parent) {}

SkyTimeController::SkyTimeController(const skygate::core::ITimeSource& timeSource, QObject* parent)
    : QObject(parent), m_utcTime(timeSource.nowUtc()), m_timeZone(initialTimeZoneOrFallback({})),
      m_timeZoneCatalogModel(new TimeZoneCatalogModel(this))
{
    m_timeZoneCatalogModel->setReferenceUtcDateTime(utcDateTime());
}

SkyTimeController::~SkyTimeController() = default;

QString SkyTimeController::dateText() const
{
    return formattedDateText();
}

QString SkyTimeController::timeText() const
{
    return formattedTimeText();
}

QString SkyTimeController::timeZoneId() const
{
    return QString::fromUtf8(m_timeZone.id());
}

QString SkyTimeController::timeZoneLabel() const
{
    return SkyQtTimeCodec::timeZoneLabel(displayDateTime());
}

QString SkyTimeController::timeZoneDetailText() const
{
    return m_timeZoneCatalogModel != nullptr ? m_timeZoneCatalogModel->detailTextForTimeZoneId(timeZoneId())
                                             : QString();
}

QAbstractItemModel* SkyTimeController::timeZoneCatalogModel() const noexcept
{
    return m_timeZoneCatalogModel;
}

QDateTime SkyTimeController::utcDateTime() const
{
    return SkyQtTimeCodec::toQDateTimeUtc(m_utcTime);
}

skygate::core::UtcTimePoint SkyTimeController::utcTimePoint() const
{
    return m_utcTime;
}

void SkyTimeController::setUtcDateTime(const QDateTime& utcDateTime)
{
    const QDateTime nextUtc = utcDateTime.toUTC();
    if (!nextUtc.isValid()) {
        return;
    }

    setUtcTimePoint(SkyQtTimeCodec::toUtcTimePoint(nextUtc));
}

void SkyTimeController::setUtcTimePoint(const skygate::core::UtcTimePoint& utcTime)
{
    const QString previousDateText = formattedDateText();
    const QString previousTimeText = formattedTimeText();
    const QString previousTimeZoneLabel = timeZoneLabel();
    const QString previousTimeZoneDetailText = timeZoneDetailText();
    if (m_utcTime == utcTime) {
        return;
    }

    m_utcTime = utcTime;
    if (m_timeZoneCatalogModel != nullptr) {
        m_timeZoneCatalogModel->setReferenceUtcDateTime(utcDateTime());
    }

    emitDisplayTextChanges(previousDateText, previousTimeText);
    if (previousTimeZoneLabel != timeZoneLabel() || previousTimeZoneDetailText != timeZoneDetailText()) {
        emit timeZoneChanged();
    }
}

bool SkyTimeController::setInitialTimeZoneId(const QString& timeZoneId)
{
    const QTimeZone nextTimeZone = initialTimeZoneOrFallback(timeZoneId);
    const QString previousDateText = formattedDateText();
    const QString previousTimeText = formattedTimeText();
    const bool changed = m_timeZone != nextTimeZone;
    m_timeZone = nextTimeZone;
    if (!changed) {
        return true;
    }

    emitDisplayTextChanges(previousDateText, previousTimeText);
    emit timeZoneChanged();
    return true;
}

QString SkyTimeController::validateDateTimeText(const QString& dateText, const QString& timeText) const
{
    return parseDisplayDateTime(dateText, timeText).errorText;
}

bool SkyTimeController::setDateTimeText(const QString& dateText, const QString& timeText)
{
    const ParseResult parseResult = parseDisplayDateTime(dateText, timeText);
    if (!parseResult.isValid()) {
        return false;
    }

    emit utcDateTimeChangeRequested(parseResult.utcDateTime);
    return true;
}

bool SkyTimeController::setTimeZoneId(const QString& timeZoneId)
{
    const std::optional<QTimeZone> nextTimeZone = availableTimeZone(timeZoneId);
    if (!nextTimeZone.has_value()) {
        return false;
    }

    if (m_timeZone == *nextTimeZone) {
        return true;
    }

    const QString previousDateText = formattedDateText();
    const QString previousTimeText = formattedTimeText();
    m_timeZone = *nextTimeZone;
    emitDisplayTextChanges(previousDateText, previousTimeText);
    emit timeZoneChanged();
    return true;
}

void SkyTimeController::goLiveNow()
{
    emit goLiveNowRequested();
}

QString SkyTimeController::formatUtcTime(const skygate::core::UtcTimePoint& utcTime, const bool includeSeconds) const
{
    const QDateTime zonedDateTime = SkyQtTimeCodec::toQDateTimeUtc(utcTime).toTimeZone(m_timeZone);
    const QString timeText = includeSeconds ? SkyQtTimeCodec::formatDateTimeText(zonedDateTime)
                                            : SkyQtTimeCodec::formatTimeText(zonedDateTime, false);
    return QString("%1 %2").arg(timeText, SkyQtTimeCodec::timeZoneLabel(zonedDateTime));
}

QString SkyTimeController::formatUtcTimeRange(
    const skygate::core::UtcTimePoint& startUtc, const skygate::core::UtcTimePoint& endUtc
) const
{
    const QDateTime startDateTime = SkyQtTimeCodec::toQDateTimeUtc(startUtc).toTimeZone(m_timeZone);
    const QDateTime endDateTime = SkyQtTimeCodec::toQDateTimeUtc(endUtc).toTimeZone(m_timeZone);
    const QString label = SkyQtTimeCodec::timeZoneLabel(startDateTime);
    if (startDateTime.date() == endDateTime.date() && SkyQtTimeCodec::timeZoneLabel(endDateTime) == label) {
        return QString("%1 %2 / %3 %4")
            .arg(
                SkyQtTimeCodec::formatDateText(startDateTime.date()),
                SkyQtTimeCodec::formatTimeText(startDateTime),
                SkyQtTimeCodec::formatTimeText(endDateTime),
                label
            );
    }

    return QString("%1 / %2").arg(formatUtcTime(startUtc), formatUtcTime(endUtc));
}

bool SkyTimeController::ParseResult::isValid() const noexcept
{
    return utcDateTime.isValid() && errorText.isEmpty();
}

QDateTime SkyTimeController::displayDateTime() const
{
    return utcDateTime().toTimeZone(m_timeZone);
}

SkyTimeController::ParseResult
SkyTimeController::parseDisplayDateTime(const QString& dateText, const QString& timeText) const
{
    const SkyQtTimeCodec::ParseResult parsed = SkyQtTimeCodec::parseDateTime(dateText, timeText);
    if (parsed.error != SkyQtTimeCodec::ParseError::None) {
        return {
            .utcDateTime = {},
            .errorText = SkyQtTimeCodec::errorText(parsed.error, SkyQtTimeCodec::ErrorScope::LocalDisplay),
        };
    }

    const QDateTime zonedDateTime = parsed.toQDateTime(m_timeZone);
    if (!zonedDateTime.isValid()) {
        return {
            .utcDateTime = {},
            .errorText =
                SkyQtTimeCodec::errorText(SkyQtTimeCodec::ParseError::None, SkyQtTimeCodec::ErrorScope::LocalDisplay),
        };
    }

    return {.utcDateTime = zonedDateTime.toUTC(), .errorText = {}};
}

QString SkyTimeController::formattedDateText() const
{
    return SkyQtTimeCodec::formatDateText(displayDateTime().date());
}

QString SkyTimeController::formattedTimeText() const
{
    return SkyQtTimeCodec::formatTimeText(displayDateTime());
}

void SkyTimeController::emitDisplayTextChanges(const QString& previousDateText, const QString& previousTimeText)
{
    if (previousDateText != formattedDateText()) {
        emit dateTextChanged();
    }
    if (previousTimeText != formattedTimeText()) {
        emit timeTextChanged();
    }
}
