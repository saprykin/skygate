#include "TimeZoneCatalogModel.hpp"

#include "TimeZoneAliasCatalog.hpp"

#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace {

QString normalizedFilterText(const QString& value)
{
    QString normalized = value.trimmed().toCaseFolded();
    normalized.replace(QLatin1Char('_'), QLatin1Char(' '));
    normalized.replace(QLatin1Char('-'), QLatin1Char(' '));
    return normalized.simplified();
}

QString humanizedTimeZoneId(const QString& timeZoneId)
{
    QString text = timeZoneId;
    text.replace(QLatin1Char('/'), QLatin1Char(' '));
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    text.replace(QLatin1Char('-'), QLatin1Char(' '));
    return text.simplified();
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

QString timeZoneLabel(const QTimeZone& timeZone, const QDateTime& utcDateTime)
{
    const QDateTime zonedDateTime = utcDateTime.toTimeZone(timeZone);
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

QString rawTimeZoneLabel(const QTimeZone& timeZone, const QDateTime& utcDateTime)
{
    const QDateTime zonedDateTime = utcDateTime.toTimeZone(timeZone);
    QStringList labels;
    labels.push_back(timeZone.abbreviation(zonedDateTime).trimmed());
    labels.push_back(timeZone.displayName(zonedDateTime, QTimeZone::ShortName).trimmed());
    labels.push_back(timeZone.displayName(zonedDateTime, QTimeZone::LongName).trimmed());
    labels.removeAll(QString());
    labels.removeDuplicates();
    return labels.join(QLatin1Char(' '));
}

QVector<TimeZoneCatalogEntry> loadTimeZoneEntries()
{
    QVector<TimeZoneCatalogEntry> entries;
    entries.push_back(TimeZoneCatalogEntry {QStringLiteral("UTC")});

    for (const QByteArray& idBytes : QTimeZone::availableTimeZoneIds()) {
        const QString id = QString::fromUtf8(idBytes);
        if (id.isEmpty() || id == QStringLiteral("UTC")) {
            continue;
        }
        entries.push_back(TimeZoneCatalogEntry {id});
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.id == QStringLiteral("UTC")) {
            return true;
        }
        if (rhs.id == QStringLiteral("UTC")) {
            return false;
        }
        return QString::compare(lhs.id, rhs.id, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

} // namespace

TimeZoneCatalogModel::TimeZoneCatalogModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_entries(loadTimeZoneEntries())
    , m_referenceUtcDateTime(QDateTime::currentDateTimeUtc())
{
    rebuildRows();
}

int TimeZoneCatalogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant TimeZoneCatalogModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case RowKindRole:
        return QStringLiteral("timeZone");
    case DisplayTextRole:
        return row.entry.id;
    case DetailTextRole:
        return detailText(row.entry);
    case TimeZoneIdRole:
        return row.entry.id;
    case SelectableRole:
        return true;
    default:
        return {};
    }
}

QHash<int, QByteArray> TimeZoneCatalogModel::roleNames() const
{
    return {
        {RowKindRole, "rowKind"},
        {DisplayTextRole, "displayText"},
        {DetailTextRole, "detailText"},
        {TimeZoneIdRole, "timeZoneId"},
        {SelectableRole, "selectable"},
    };
}

QString TimeZoneCatalogModel::filterText() const
{
    return m_filterText;
}

void TimeZoneCatalogModel::setFilterText(const QString& filterText)
{
    const QString normalized = normalizedFilterText(filterText);
    if (m_filterText == normalized) {
        return;
    }

    m_filterText = normalized;
    rebuildRows();
    emit filterTextChanged();
}

bool TimeZoneCatalogModel::hasTimeZoneId(const QString& timeZoneId) const
{
    const QString id = timeZoneId.trimmed();
    return std::any_of(m_entries.cbegin(), m_entries.cend(), [&id](const auto& entry) {
        return entry.id == id;
    });
}

QString TimeZoneCatalogModel::detailTextForTimeZoneId(const QString& timeZoneId) const
{
    const QString id = timeZoneId.trimmed();
    const auto entryIt = std::find_if(m_entries.cbegin(), m_entries.cend(), [&id](const auto& entry) {
        return entry.id == id;
    });
    if (entryIt == m_entries.cend()) {
        return {};
    }
    return detailText(*entryIt);
}

void TimeZoneCatalogModel::setReferenceUtcDateTime(const QDateTime& utcDateTime)
{
    const QDateTime nextUtc = utcDateTime.toUTC();
    if (m_referenceUtcDateTime == nextUtc) {
        return;
    }

    m_referenceUtcDateTime = nextUtc;
    if (!m_filterText.isEmpty()) {
        rebuildRows();
        return;
    }

    if (m_rows.isEmpty()) {
        return;
    }

    emit dataChanged(index(0, 0), index(m_rows.size() - 1, 0), {DetailTextRole});
}

void TimeZoneCatalogModel::rebuildRows()
{
    beginResetModel();
    m_rows.clear();
    for (const TimeZoneCatalogEntry& entry : m_entries) {
        if (matchesFilter(entry)) {
            m_rows.push_back(Row {.entry = entry});
        }
    }
    endResetModel();
}

QString TimeZoneCatalogModel::detailText(const TimeZoneCatalogEntry& entry) const
{
    const QTimeZone timeZone(entry.id.toUtf8());
    if (!timeZone.isValid()) {
        return {};
    }

    const QDateTime utc = m_referenceUtcDateTime.isValid()
        ? m_referenceUtcDateTime.toUTC()
        : QDateTime::currentDateTimeUtc();
    const QDateTime zonedDateTime = utc.toTimeZone(timeZone);
    const QString label = timeZoneLabel(timeZone, utc);
    const QString offsetText = formatUtcOffset(timeZone.offsetFromUtc(zonedDateTime));
    return label == offsetText
        ? offsetText
        : QString("%1 · %2").arg(label, offsetText);
}

bool TimeZoneCatalogModel::matchesFilter(const TimeZoneCatalogEntry& entry) const
{
    if (m_filterText.isEmpty()) {
        return true;
    }

    const QString id = normalizedFilterText(entry.id);
    if (id.contains(m_filterText)) {
        return true;
    }

    const QString humanizedId = normalizedFilterText(humanizedTimeZoneId(entry.id));
    if (humanizedId.contains(m_filterText)) {
        return true;
    }

    const QString aliases = normalizedFilterText(
        TimeZoneAliasCatalog::aliasesForTimeZoneId(entry.id).join(QLatin1Char(' '))
    );
    if (aliases.contains(m_filterText)) {
        return true;
    }

    const QString detail = normalizedFilterText(detailText(entry));
    if (detail.contains(m_filterText)) {
        return true;
    }

    const QTimeZone timeZone(entry.id.toUtf8());
    if (!timeZone.isValid()) {
        return false;
    }

    const QDateTime referenceUtc = m_referenceUtcDateTime.isValid()
        ? m_referenceUtcDateTime.toUTC()
        : QDateTime::currentDateTimeUtc();
    const QString seasonalLabels = normalizedFilterText(
        rawTimeZoneLabel(timeZone, QDateTime(QDate(2026, 1, 15), QTime(12, 0, 0), QTimeZone::UTC))
        + QLatin1Char(' ')
        + rawTimeZoneLabel(timeZone, QDateTime(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC))
        + QLatin1Char(' ')
        + rawTimeZoneLabel(timeZone, referenceUtc)
    );
    return seasonalLabels.contains(m_filterText);
}
