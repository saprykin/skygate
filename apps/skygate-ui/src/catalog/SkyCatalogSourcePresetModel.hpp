#pragma once

#include "SkyCatalogSourceDescriptor.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QVector>

namespace skygate::ui::internal {

// Read-only list model that exposes catalog source presets to QML. When
// includeCustomOption is set, a trailing "Custom URL" row is appended so the
// fixed legacy custom index remains the final row.
class SkyCatalogSourcePresetModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        SourceIdRole,
        VersionRole,
        DefaultUrlRole,
        AttributionRole,
        LegacyPresetIndexRole,
        BundledRole,
        CustomRole,
    };

    explicit SkyCatalogSourcePresetModel(
        QVector<SkyCatalogSourceDescriptor> descriptors, bool includeCustomOption, QObject* parent = nullptr
    );

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] Q_INVOKABLE QString sourceIdAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString titleAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString defaultUrlAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString versionAt(int row) const;
    [[nodiscard]] Q_INVOKABLE int legacyPresetIndexAt(int row) const;
    [[nodiscard]] Q_INVOKABLE bool isCustomAt(int row) const;

private:
    struct Row final {
        SkyCatalogSourceDescriptor descriptor;
        bool custom = false;
    };

    [[nodiscard]] const Row* rowAt(int row) const;

private:
    QVector<Row> m_rows;
};

}  // namespace skygate::ui::internal
