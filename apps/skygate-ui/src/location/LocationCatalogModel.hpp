#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

#include <optional>

struct LocationCatalogEntry final {
    QString id;
    QString countryName;
    QString cityName;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;

    [[nodiscard]] QString displayText() const;
};

class LocationCatalogModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Roles {
        RowKindRole = Qt::UserRole + 1,
        DisplayTextRole,
        DetailTextRole,
        CityIdRole,
        CountryNameRole,
        CityNameRole,
        LatitudeDegRole,
        LongitudeDegRole,
        SelectableRole,
    };

    explicit LocationCatalogModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString filterText() const;
    void setFilterText(const QString& filterText);

    [[nodiscard]] bool hasCityId(const QString& cityId) const;
    [[nodiscard]] std::optional<LocationCatalogEntry> entryForCityId(
        const QString& cityId
    ) const;

signals:
    void filterTextChanged();

private:
    enum class RowKind {
        CountryHeader,
        City,
    };

    struct Row final {
        RowKind kind = RowKind::CountryHeader;
        QString displayText;
        QString detailText;
        LocationCatalogEntry entry;
    };

private:
    [[nodiscard]] bool loadCatalog();
    void rebuildRows();

private:
    QVector<LocationCatalogEntry> m_entries;
    QVector<Row> m_rows;
    QString m_filterText;
};
