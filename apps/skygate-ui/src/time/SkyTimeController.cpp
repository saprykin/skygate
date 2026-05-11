#include "SkyTimeController.hpp"

#include "TimeZoneCatalogModel.hpp"

#include "skygate/core/ITimeSource.hpp"
#include "skygate/core/SystemTimeSource.hpp"

#include <QDate>
#include <QRegularExpression>
#include <QTime>
#include <QTimeZone>

#include <cmath>
#include <chrono>

namespace {

const QRegularExpression& dateInputPattern()
{
    static const QRegularExpression pattern(
        R"(^(\d{4,})-(\d{2})-(\d{2})(?:\s+(BCE|BC))?$)",
        QRegularExpression::CaseInsensitiveOption
    );
    return pattern;
}

QString formatDateText(const QDate& date)
{
    const int year = date.year();
    const qint64 displayYear = year < 0
        ? -static_cast<qint64>(year)
        : static_cast<qint64>(year);
    const QString yearText = QString::number(displayYear).rightJustified(4, '0');
    const QString dateText = QString("%1-%2-%3")
        .arg(yearText)
        .arg(date.month(), 2, 10, QLatin1Char('0'))
        .arg(date.day(), 2, 10, QLatin1Char('0'));
    return year < 0
        ? QString("%1 BCE").arg(dateText)
        : dateText;
}

QString formatUtcOffset(const int offsetSeconds)
{
    const QChar sign = offsetSeconds < 0 ? QLatin1Char('-') : QLatin1Char('+');
    const int absoluteSeconds = std::abs(offsetSeconds);
    return QString("UTC%1%2:%3")
        .arg(sign)
        .arg(absoluteSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((absoluteSeconds % 3600) / 60, 2, 10, QLatin1Char('0'));
}

QTimeZone validTimeZoneOrFallback(const QString& timeZoneId)
{
    const QByteArray idBytes = timeZoneId.trimmed().toUtf8();
    if (!idBytes.isEmpty() && QTimeZone::isTimeZoneIdAvailable(idBytes)) {
        return QTimeZone(idBytes);
    }

    const QByteArray systemId = QTimeZone::systemTimeZoneId();
    if (!systemId.isEmpty() && QTimeZone::isTimeZoneIdAvailable(systemId)) {
        return QTimeZone(systemId);
    }

    return QTimeZone::utc();
}

const skygate::core::SystemTimeSource& defaultTimeSource()
{
    static const skygate::core::SystemTimeSource timeSource;
    return timeSource;
}

QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        utcTime.time_since_epoch()
    );
    return QDateTime::fromSecsSinceEpoch(seconds.count(), QTimeZone::UTC);
}

QString timeZoneLabel(const QTimeZone& timeZone, const QDateTime& zonedDateTime)
{
    QString label = timeZone.abbreviation(zonedDateTime).trimmed();
    if (label.isEmpty()) {
        label = timeZone.displayName(zonedDateTime, QTimeZone::ShortName).trimmed();
    }

    const QString offsetText = formatUtcOffset(timeZone.offsetFromUtc(zonedDateTime));
    if (
        label.isEmpty()
        || label.startsWith(QLatin1String("GMT"))
        || label.startsWith(QLatin1Char('+'))
        || label.startsWith(QLatin1Char('-'))
    ) {
        return offsetText;
    }
    return label;
}

} // namespace

SkyTimeController::SkyTimeController(QObject* parent)
    : SkyTimeController(defaultTimeSource(), parent)
{
}

SkyTimeController::SkyTimeController(
    const skygate::core::ITimeSource& timeSource,
    QObject* parent
)
    : QObject(parent)
    , m_utcDateTime(toQDateTimeUtc(timeSource.nowUtc()))
    , m_timeZone(validTimeZoneOrFallback({}))
    , m_timeZoneCatalogModel(new TimeZoneCatalogModel(this))
{
    m_timeZoneCatalogModel->setReferenceUtcDateTime(m_utcDateTime);
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
    return ::timeZoneLabel(m_timeZone, displayDateTime());
}

QString SkyTimeController::timeZoneDetailText() const
{
    return m_timeZoneCatalogModel != nullptr
        ? m_timeZoneCatalogModel->detailTextForTimeZoneId(timeZoneId())
        : QString();
}

QAbstractItemModel* SkyTimeController::timeZoneCatalogModel() const noexcept
{
    return m_timeZoneCatalogModel;
}

QDateTime SkyTimeController::utcDateTime() const
{
    return m_utcDateTime;
}

skygate::core::UtcTimePoint SkyTimeController::utcTimePoint() const
{
    return skygate::core::UtcTimePoint(std::chrono::seconds(m_utcDateTime.toSecsSinceEpoch()));
}

void SkyTimeController::setUtcDateTime(const QDateTime& utcDateTime)
{
    const QDateTime nextUtc = utcDateTime.toUTC();
    if (!nextUtc.isValid()) {
        return;
    }

    const QString previousDateText = formattedDateText();
    const QString previousTimeText = formattedTimeText();
    const QString previousTimeZoneLabel = timeZoneLabel();
    const QString previousTimeZoneDetailText = timeZoneDetailText();
    if (m_utcDateTime == nextUtc) {
        return;
    }

    m_utcDateTime = nextUtc;
    if (m_timeZoneCatalogModel != nullptr) {
        m_timeZoneCatalogModel->setReferenceUtcDateTime(m_utcDateTime);
    }

    emitDisplayTextChanges(previousDateText, previousTimeText);
    if (
        previousTimeZoneLabel != timeZoneLabel()
        || previousTimeZoneDetailText != timeZoneDetailText()
    ) {
        emit timeZoneChanged();
    }
}

void SkyTimeController::setUtcTimePoint(const skygate::core::UtcTimePoint& utcTime)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        utcTime.time_since_epoch()
    );
    setUtcDateTime(QDateTime::fromSecsSinceEpoch(seconds.count(), QTimeZone::UTC));
}

bool SkyTimeController::setInitialTimeZoneId(const QString& timeZoneId)
{
    const QByteArray idBytes = timeZoneId.trimmed().toUtf8();
    const QTimeZone nextTimeZone = idBytes.isEmpty()
        ? validTimeZoneOrFallback({})
        : (
            QTimeZone::isTimeZoneIdAvailable(idBytes)
                ? QTimeZone(idBytes)
                : QTimeZone::utc()
        );
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

QString SkyTimeController::validateDateTimeText(
    const QString& dateText,
    const QString& timeText
) const
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
    const QByteArray idBytes = timeZoneId.trimmed().toUtf8();
    if (idBytes.isEmpty() || !QTimeZone::isTimeZoneIdAvailable(idBytes)) {
        return false;
    }

    const QTimeZone nextTimeZone(idBytes);
    if (m_timeZone == nextTimeZone) {
        return true;
    }

    const QString previousDateText = formattedDateText();
    const QString previousTimeText = formattedTimeText();
    m_timeZone = nextTimeZone;
    emitDisplayTextChanges(previousDateText, previousTimeText);
    emit timeZoneChanged();
    return true;
}

void SkyTimeController::goLiveNow()
{
    emit goLiveNowRequested();
}

QString SkyTimeController::formatUtcTime(
    const skygate::core::UtcTimePoint& utcTime,
    const bool includeSeconds
) const
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        utcTime.time_since_epoch()
    );
    const QDateTime zonedDateTime = QDateTime::fromSecsSinceEpoch(
        seconds.count(),
        QTimeZone::UTC
    ).toTimeZone(m_timeZone);
    return includeSeconds
        ? QString("%1 %2").arg(
            zonedDateTime.toString("yyyy-MM-dd HH:mm:ss"),
            ::timeZoneLabel(m_timeZone, zonedDateTime)
        )
        : QString("%1 %2").arg(
            zonedDateTime.toString("HH:mm"),
            ::timeZoneLabel(m_timeZone, zonedDateTime)
        );
}

QString SkyTimeController::formatUtcTimeRange(
    const skygate::core::UtcTimePoint& startUtc,
    const skygate::core::UtcTimePoint& endUtc
) const
{
    const auto startSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        startUtc.time_since_epoch()
    );
    const auto endSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        endUtc.time_since_epoch()
    );
    const QDateTime startDateTime = QDateTime::fromSecsSinceEpoch(
        startSeconds.count(),
        QTimeZone::UTC
    ).toTimeZone(m_timeZone);
    const QDateTime endDateTime = QDateTime::fromSecsSinceEpoch(
        endSeconds.count(),
        QTimeZone::UTC
    ).toTimeZone(m_timeZone);
    const QString label = ::timeZoneLabel(m_timeZone, startDateTime);
    if (
        startDateTime.date() == endDateTime.date()
        && ::timeZoneLabel(m_timeZone, endDateTime) == label
    ) {
        return QString("%1 %2 / %3 %4").arg(
            startDateTime.toString("yyyy-MM-dd"),
            startDateTime.toString("HH:mm:ss"),
            endDateTime.toString("HH:mm:ss"),
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
    return m_utcDateTime.toTimeZone(m_timeZone);
}

SkyTimeController::ParseResult SkyTimeController::parseDisplayDateTime(
    const QString& dateText,
    const QString& timeText
) const
{
    const QRegularExpressionMatch dateMatch = dateInputPattern().match(dateText.trimmed());
    if (!dateMatch.hasMatch()) {
        return {
            .utcDateTime = {},
            .errorText = "Use YYYY-MM-DD for CE or YYYY-MM-DD BCE for ancient dates."
        };
    }

    bool isValidYear = false;
    const int yearMagnitude = dateMatch.captured(1).toInt(&isValidYear);
    if (!isValidYear || yearMagnitude == 0) {
        return {
            .utcDateTime = {},
            .errorText = "Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE."
        };
    }

    bool isValidMonth = false;
    const int month = dateMatch.captured(2).toInt(&isValidMonth);
    bool isValidDay = false;
    const int day = dateMatch.captured(3).toInt(&isValidDay);
    if (!isValidMonth || !isValidDay) {
        return {
            .utcDateTime = {},
            .errorText = "Enter a valid date."
        };
    }

    const bool isBce = !dateMatch.captured(4).isEmpty();
    const int year = isBce ? -yearMagnitude : yearMagnitude;
    const QDate date(year, month, day);
    if (!date.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = "Enter a valid date."
        };
    }

    const QTime time = QTime::fromString(timeText.trimmed(), "HH:mm:ss");
    if (!time.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = "Use HH:mm:ss for the time."
        };
    }

    const QDateTime zonedDateTime(date, time, m_timeZone);
    if (!zonedDateTime.isValid()) {
        return {
            .utcDateTime = {},
            .errorText = "Enter a valid date and time."
        };
    }

    return {
        .utcDateTime = zonedDateTime.toUTC(),
        .errorText = {}
    };
}

QString SkyTimeController::formattedDateText() const
{
    return formatDateText(displayDateTime().date());
}

QString SkyTimeController::formattedTimeText() const
{
    return displayDateTime().toString("HH:mm:ss");
}

void SkyTimeController::emitDisplayTextChanges(
    const QString& previousDateText,
    const QString& previousTimeText
)
{
    if (previousDateText != formattedDateText()) {
        emit dateTextChanged();
    }
    if (previousTimeText != formattedTimeText()) {
        emit timeTextChanged();
    }
}
