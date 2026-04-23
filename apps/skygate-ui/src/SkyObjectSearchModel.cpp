#include "SkyObjectSearchModel.hpp"

#include <algorithm>
#include <cmath>

namespace {

QString normalizedKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

QString compactedKey(const QString& value)
{
    const QString normalized = normalizedKey(value);
    QString compacted;
    compacted.reserve(normalized.size());
    for (const QChar character : normalized) {
        if (character.isSpace() || character == '_' || character == '-') {
            continue;
        }

        compacted.append(character);
    }
    return compacted;
}

QString celestialBodyTypeText(const skygate::ephemeris::CelestialBodyType type)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return "Sun";
    case skygate::ephemeris::CelestialBodyType::Moon:
        return "Moon";
    case skygate::ephemeris::CelestialBodyType::Planet:
        return "Planet";
    case skygate::ephemeris::CelestialBodyType::Star:
        return "Star";
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return "Constellation";
    case skygate::ephemeris::CelestialBodyType::DeepSkyObject:
        return "Deep sky";
    }

    return "Object";
}

bool shouldShowBodyId(const skygate::ephemeris::CelestialBody& body)
{
    if (body.id.empty()) {
        return false;
    }

    const QString bodyId = QString::fromStdString(body.id);
    for (const QChar character : bodyId) {
        if (character == '_' || character == '-' || character.isDigit()) {
            return true;
        }
    }

    return false;
}

QString bodyDetailText(const skygate::ephemeris::CelestialBody& body)
{
    const QString typeText = celestialBodyTypeText(body.type);
    if (body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
        QString detailText = typeText;
        if (body.deepSkyObject.has_value()) {
            switch (body.deepSkyObject->kind) {
            case skygate::ephemeris::DeepSkyObjectKind::Galaxy:
                detailText = "Deep sky • Galaxy";
                break;
            case skygate::ephemeris::DeepSkyObjectKind::OpenCluster:
                detailText = "Deep sky • Open cluster";
                break;
            case skygate::ephemeris::DeepSkyObjectKind::GlobularCluster:
                detailText = "Deep sky • Globular cluster";
                break;
            case skygate::ephemeris::DeepSkyObjectKind::Nebula:
                detailText = "Deep sky • Nebula";
                break;
            case skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula:
                detailText = "Deep sky • Planetary nebula";
                break;
            case skygate::ephemeris::DeepSkyObjectKind::Asterism:
                detailText = "Deep sky • Asterism";
                break;
            case skygate::ephemeris::DeepSkyObjectKind::Unknown:
                break;
            }
        }

        if (!shouldShowBodyId(body)) {
            return detailText;
        }
        return QString("%1 • %2").arg(detailText, QString::fromStdString(body.id));
    }

    if (!shouldShowBodyId(body)) {
        return typeText;
    }

    return QString("%1 • %2").arg(typeText, QString::fromStdString(body.id));
}

}  // namespace

SkyObjectSearchModel::SkyObjectSearchModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SkyObjectSearchModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_rows.size();
}

QVariant SkyObjectSearchModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const SourceEntry& entry = m_sourceEntries.at(m_rows.at(index.row()).sourceIndex);
    switch (role) {
    case DisplayTextRole:
        return entry.displayText;
    case DetailTextRole:
        return entry.detailText;
    case TargetKindRole:
        return entry.targetKind;
    case TargetIdRole:
        return entry.targetId;
    case SelectableRole:
        return entry.selectable;
    default:
        return {};
    }
}

QHash<int, QByteArray> SkyObjectSearchModel::roleNames() const
{
    return {
        {DisplayTextRole, "displayText"},
        {DetailTextRole, "detailText"},
        {TargetKindRole, "targetKind"},
        {TargetIdRole, "targetId"},
        {SelectableRole, "selectable"},
    };
}

QString SkyObjectSearchModel::filterText() const
{
    return m_filterText;
}

void SkyObjectSearchModel::setFilterText(const QString& filterText)
{
    if (m_filterText == filterText) {
        return;
    }

    m_filterText = filterText;
    rebuildRows();
    emit filterTextChanged();
}

void SkyObjectSearchModel::setCatalogData(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs
)
{
    beginResetModel();

    m_sourceEntries.clear();
    m_rows.clear();

    QHash<QString, bool> availableBodyIds;
    availableBodyIds.reserve(static_cast<int>(bodies.size()));
    for (const auto& body : bodies) {
        if (body.id.empty()) {
            continue;
        }

        availableBodyIds.insert(normalizedKey(QString::fromStdString(body.id)), true);
    }

    QHash<QString, int> sourceIndexByDisplayKey;
    const auto shouldReplaceEntry = [](const SourceEntry& existing, const SourceEntry& candidate) {
        if (existing.isBody != candidate.isBody) {
            return candidate.isBody;
        }

        if (candidate.brightnessMagnitude < existing.brightnessMagnitude - 1e-9) {
            return true;
        }
        if (candidate.brightnessMagnitude > existing.brightnessMagnitude + 1e-9) {
            return false;
        }

        const int displayCompare = QString::compare(
            candidate.displayText,
            existing.displayText,
            Qt::CaseInsensitive
        );
        if (displayCompare != 0) {
            return displayCompare < 0;
        }

        return QString::compare(candidate.targetId, existing.targetId, Qt::CaseInsensitive) < 0;
    };
    const auto upsertEntry = [this, &sourceIndexByDisplayKey, &shouldReplaceEntry](SourceEntry entry) {
        entry.displayText = entry.displayText.trimmed();
        entry.targetId = entry.targetId.trimmed();
        if (entry.displayText.isEmpty() || entry.targetId.isEmpty()) {
            return;
        }

        entry.displayKey = normalizedKey(entry.displayText);
        entry.compactDisplayKey = compactedKey(entry.displayText);
        entry.targetIdKey = normalizedKey(entry.targetId);
        entry.compactTargetIdKey = compactedKey(entry.targetId);

        const auto existingIt = sourceIndexByDisplayKey.constFind(entry.displayKey);
        if (existingIt == sourceIndexByDisplayKey.cend()) {
            sourceIndexByDisplayKey.insert(entry.displayKey, m_sourceEntries.size());
            m_sourceEntries.push_back(std::move(entry));
            return;
        }

        if (shouldReplaceEntry(m_sourceEntries[*existingIt], entry)) {
            m_sourceEntries[*existingIt] = std::move(entry);
        }
    };

    for (const auto& body : bodies) {
        if (body.displayName.empty() || body.id.empty()) {
            continue;
        }

        const QString targetId = QString::fromStdString(body.id);
        upsertEntry(SourceEntry {
            .displayText = QString::fromStdString(body.displayName),
            .detailText = bodyDetailText(body),
            .targetKind = "body",
            .targetId = targetId,
            .brightnessMagnitude = body.visualMagnitude,
            .selectable = true,
            .isBody = true,
        });

        if (body.type != skygate::ephemeris::CelestialBodyType::DeepSkyObject
            || !body.deepSkyObject.has_value()) {
            continue;
        }

        for (const std::string& alias : body.deepSkyObject->aliases) {
            if (alias.empty() || alias == body.displayName) {
                continue;
            }

            upsertEntry(SourceEntry {
                .displayText = QString::fromStdString(alias),
                .detailText = bodyDetailText(body),
                .targetKind = "body",
                .targetId = targetId,
                .brightnessMagnitude = body.visualMagnitude,
                .selectable = true,
                .isBody = true,
            });
        }
    }

    for (const auto& labelRef : labelRefs) {
        if (labelRef.first.empty()) {
            continue;
        }

        bool hasAnyAnchor = false;
        for (const std::string& hipId : labelRef.second) {
            if (availableBodyIds.contains(normalizedKey(QString::fromStdString(hipId)))) {
                hasAnyAnchor = true;
                break;
            }
        }
        if (!hasAnyAnchor) {
            continue;
        }

        upsertEntry(SourceEntry {
            .displayText = QString::fromStdString(labelRef.first),
            .detailText = "Constellation",
            .targetKind = "constellationLabel",
            .targetId = QString::fromStdString(labelRef.first),
            .brightnessMagnitude = 99.0,
            .selectable = true,
            .isBody = false,
        });
    }

    endResetModel();
    rebuildRows();
}

void SkyObjectSearchModel::rebuildRows()
{
    beginResetModel();

    m_rows.clear();
    const QString filterKey = normalizedKey(m_filterText);
    const QString compactFilterKey = compactedKey(m_filterText);
    if (filterKey.isEmpty()) {
        endResetModel();
        return;
    }

    for (int sourceIndex = 0; sourceIndex < m_sourceEntries.size(); ++sourceIndex) {
        const SourceEntry& entry = m_sourceEntries.at(sourceIndex);
        int entryMatchRank = -1;
        const bool matchesExactName =
            entry.displayKey == filterKey || entry.compactDisplayKey == compactFilterKey;
        if (matchesExactName) {
            entryMatchRank = 0;
        } else {
            const bool matchesPrefixName =
                entry.displayKey.startsWith(filterKey)
                || entry.compactDisplayKey.startsWith(compactFilterKey);
            if (matchesPrefixName) {
                entryMatchRank = 1;
            } else {
                const bool matchesPrefixId =
                    entry.targetIdKey.startsWith(filterKey)
                    || entry.compactTargetIdKey.startsWith(compactFilterKey);
                if (matchesPrefixId) {
                    entryMatchRank = 2;
                } else {
                    const bool matchesContains =
                        entry.displayKey.contains(filterKey)
                        || entry.compactDisplayKey.contains(compactFilterKey)
                        || entry.targetIdKey.contains(filterKey)
                        || entry.compactTargetIdKey.contains(compactFilterKey);
                    if (matchesContains) {
                        entryMatchRank = 3;
                    }
                }
            }
        }

        if (entryMatchRank < 0) {
            continue;
        }

        m_rows.push_back(Row {
            .sourceIndex = sourceIndex,
            .matchRank = entryMatchRank,
        });
    }

    std::sort(m_rows.begin(), m_rows.end(), [this](const Row& lhs, const Row& rhs) {
        if (lhs.matchRank != rhs.matchRank) {
            return lhs.matchRank < rhs.matchRank;
        }

        const SourceEntry& lhsEntry = m_sourceEntries.at(lhs.sourceIndex);
        const SourceEntry& rhsEntry = m_sourceEntries.at(rhs.sourceIndex);
        if (std::abs(lhsEntry.brightnessMagnitude - rhsEntry.brightnessMagnitude) > 1e-9) {
            return lhsEntry.brightnessMagnitude < rhsEntry.brightnessMagnitude;
        }

        const int displayCompare = QString::compare(
            lhsEntry.displayText,
            rhsEntry.displayText,
            Qt::CaseInsensitive
        );
        if (displayCompare != 0) {
            return displayCompare < 0;
        }

        return QString::compare(lhsEntry.targetId, rhsEntry.targetId, Qt::CaseInsensitive) < 0;
    });

    endResetModel();
}
