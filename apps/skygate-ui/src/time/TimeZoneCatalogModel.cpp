#include "TimeZoneCatalogModel.hpp"
#include "TimeZoneCatalogEntry.hpp"

#include "AsciiText.hpp"
#include "SkyQtTimeCodec.hpp"
#include "TimeZoneAliasCatalog.hpp"

#include <QStringList>
#include <QTimeZone>

#include <algorithm>
#include <string>

namespace {

// Case folding and trimming come from the shared lookup-key helper; the
// separator folding and whitespace collapsing stay local because time zone
// ids and labels use '_' and '-' as word breaks (America/Los_Angeles).
QString normalizedFilterKey(const QString& value)
{
    QString text = value;
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    text.replace(QLatin1Char('-'), QLatin1Char(' '));
    const std::string normalized = skygate::core::AsciiText::normalizedLookupKey(text.toStdString());
    return QString::fromStdString(normalized).simplified();
}

QString humanizedTimeZoneId(const QString& timeZoneId)
{
    QString text = timeZoneId;
    text.replace(QLatin1Char('/'), QLatin1Char(' '));
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    text.replace(QLatin1Char('-'), QLatin1Char(' '));
    return text.simplified();
}

QStringList seasonalTimeZoneLabels(const QTimeZone& timeZone, const QDateTime& referenceUtc)
{
    // Abbreviation probes keep seasonal ids such as CET and CEST searchable
    // outside the displayed season. The current-season label comes from the
    // shared codec so the offset fallback stays consistent with the details.
    const QDateTime winterUtc(QDate(2026, 1, 15), QTime(12, 0, 0), QTimeZone::UTC);
    const QDateTime summerUtc(QDate(2026, 7, 15), QTime(12, 0, 0), QTimeZone::UTC);
    return {
        timeZone.abbreviation(winterUtc.toTimeZone(timeZone)).trimmed(),
        timeZone.abbreviation(summerUtc.toTimeZone(timeZone)).trimmed(),
        SkyQtTimeCodec::timeZoneLabel(referenceUtc.toTimeZone(timeZone)),
    };
}

QVector<TimeZoneCatalogEntry> loadTimeZoneEntries()
{
    QVector<TimeZoneCatalogEntry> entries;
    entries.push_back(TimeZoneCatalogEntry{QStringLiteral("UTC")});

    for (const QByteArray& idBytes : QTimeZone::availableTimeZoneIds()) {
        const QString id = QString::fromUtf8(idBytes);
        if (id.isEmpty() || id == QStringLiteral("UTC")) {
            continue;
        }
        entries.push_back(TimeZoneCatalogEntry{id});
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

}  // namespace

TimeZoneCatalogModel::TimeZoneCatalogModel(QObject* parent)
    : QAbstractListModel(parent), m_entries(loadTimeZoneEntries()),
      m_referenceUtcDateTime(QDateTime::currentDateTimeUtc())
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

    const TimeZoneCatalogEntry& entry = m_rows.at(index.row());
    switch (role) {
    case RowKindRole:
        return QStringLiteral("timeZone");
    case DisplayTextRole:
        return entry.id;
    case DetailTextRole:
        return detailText(entry);
    case TimeZoneIdRole:
        return entry.id;
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
    const QString normalized = normalizedFilterKey(filterText);
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
    return std::any_of(m_entries.cbegin(), m_entries.cend(), [&id](const auto& entry) { return entry.id == id; });
}

QString TimeZoneCatalogModel::detailTextForTimeZoneId(const QString& timeZoneId) const
{
    const QString id = timeZoneId.trimmed();
    const auto entryIt =
        std::find_if(m_entries.cbegin(), m_entries.cend(), [&id](const auto& entry) { return entry.id == id; });
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
            m_rows.push_back(entry);
        }
    }
    endResetModel();
}

QDateTime TimeZoneCatalogModel::referenceUtcDateTime() const
{
    return m_referenceUtcDateTime.isValid() ? m_referenceUtcDateTime.toUTC() : QDateTime::currentDateTimeUtc();
}

QString TimeZoneCatalogModel::detailText(const TimeZoneCatalogEntry& entry) const
{
    const QTimeZone timeZone(entry.id.toUtf8());
    if (!timeZone.isValid()) {
        return {};
    }

    const QDateTime zonedDateTime = referenceUtcDateTime().toTimeZone(timeZone);
    const QString label = SkyQtTimeCodec::timeZoneLabel(zonedDateTime);
    const QString offsetLabel = SkyQtTimeCodec::formatUtcOffsetLabel(timeZone.offsetFromUtc(zonedDateTime));
    return label == offsetLabel ? offsetLabel : QString("%1 · %2").arg(label, offsetLabel);
}

bool TimeZoneCatalogModel::matchesFilter(const TimeZoneCatalogEntry& entry) const
{
    if (m_filterText.isEmpty()) {
        return true;
    }

    const QString id = normalizedFilterKey(entry.id);
    if (id.contains(m_filterText)) {
        return true;
    }

    const QString humanizedId = normalizedFilterKey(humanizedTimeZoneId(entry.id));
    if (humanizedId.contains(m_filterText)) {
        return true;
    }

    const QString aliases =
        normalizedFilterKey(TimeZoneAliasCatalog::aliasesForTimeZoneId(entry.id).join(QLatin1Char(' ')));
    if (aliases.contains(m_filterText)) {
        return true;
    }

    const QString detail = normalizedFilterKey(detailText(entry));
    if (detail.contains(m_filterText)) {
        return true;
    }

    const QTimeZone timeZone(entry.id.toUtf8());
    if (!timeZone.isValid()) {
        return false;
    }

    const QDateTime referenceUtc = referenceUtcDateTime();
    const QString seasonalLabels =
        normalizedFilterKey(seasonalTimeZoneLabels(timeZone, referenceUtc).join(QLatin1Char(' ')));
    return seasonalLabels.contains(m_filterText);
}
