#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <span>

class SkyObjectSearchModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Roles {
        DisplayTextRole = Qt::UserRole + 1,
        DetailTextRole,
        TargetKindRole,
        TargetIdRole,
        SelectableRole,
    };

    explicit SkyObjectSearchModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString filterText() const;
    void setFilterText(const QString& filterText);
    void setCatalogData(
        std::span<const skygate::ephemeris::CelestialBody> bodies,
        std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs
    );

signals:
    void filterTextChanged();

private:
    struct SourceEntry final {
        QString displayText;
        QString detailText;
        QString targetKind;
        QString targetId;
        QString displayKey;
        QString compactDisplayKey;
        QString targetIdKey;
        QString compactTargetIdKey;
        double brightnessMagnitude = 99.0;
        bool selectable = true;
        bool isBody = false;
    };

    struct Row final {
        int sourceIndex = 0;
        int matchRank = 0;
    };

private:
    void rebuildRows();

private:
    QVector<SourceEntry> m_sourceEntries;
    QVector<Row> m_rows;
    QString m_filterText;
};
