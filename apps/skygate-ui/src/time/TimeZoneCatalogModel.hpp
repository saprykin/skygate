#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

struct TimeZoneCatalogEntry final {
    QString id;
};

class TimeZoneCatalogModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Roles {
        RowKindRole = Qt::UserRole + 1,
        DisplayTextRole,
        DetailTextRole,
        TimeZoneIdRole,
        SelectableRole,
    };

    explicit TimeZoneCatalogModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString filterText() const;
    void setFilterText(const QString& filterText);

    [[nodiscard]] bool hasTimeZoneId(const QString& timeZoneId) const;
    [[nodiscard]] QString detailTextForTimeZoneId(const QString& timeZoneId) const;
    void setReferenceUtcDateTime(const QDateTime& utcDateTime);

signals:
    void filterTextChanged();

private:
    struct Row final {
        TimeZoneCatalogEntry entry;
    };

private:
    void rebuildRows();
    [[nodiscard]] QString detailText(const TimeZoneCatalogEntry& entry) const;
    [[nodiscard]] bool matchesFilter(const TimeZoneCatalogEntry& entry) const;

private:
    QVector<TimeZoneCatalogEntry> m_entries;
    QVector<Row> m_rows;
    QString m_filterText;
    QDateTime m_referenceUtcDateTime;
};
