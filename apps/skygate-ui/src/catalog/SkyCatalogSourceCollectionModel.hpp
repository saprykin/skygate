#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace skygate::ui::internal {

// Read-only list model that exposes the active catalog source collection to
// QML. Rows are ordered by composition precedence. The instance ID is the
// durable identity; the title is presentation text and may change without
// changing the row identity.
class SkyCatalogSourceCollectionModel final : public QAbstractListModel {
    Q_OBJECT

public:
    struct SourceEntry final {
        QString instanceId;
        QString title;
        QString version;
        QString category;
        bool enabled = true;
        bool bundled = false;
        bool busy = false;
        bool hasError = false;
        QString statusText;
        int objectCount = 0;
    };

    enum Roles {
        InstanceIdRole = Qt::UserRole + 1,
        TitleRole,
        VersionRole,
        CategoryRole,
        EnabledRole,
        BundledRole,
        BusyRole,
        HasErrorRole,
        StatusRole,
        ObjectCountRole,
    };

    explicit SkyCatalogSourceCollectionModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void replaceEntries(QVector<SourceEntry> entries);

    [[nodiscard]] int indexOfInstanceId(const QString& instanceId) const;

private:
    QVector<SourceEntry> m_entries;
};

}  // namespace skygate::ui::internal
