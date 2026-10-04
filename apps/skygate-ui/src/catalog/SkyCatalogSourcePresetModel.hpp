#pragma once

#include "SkyCatalogSourceDescriptor.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace skygate::ui::internal {

// Read-only list model that exposes catalog source presets to QML. Preset rows
// carry the descriptor metadata plus an optional user-facing category. Custom
// rows have no stable source ID and instead carry a category that selects the
// composition policy when the user supplies a URL.
class SkyCatalogSourcePresetModel final : public QAbstractListModel {
    Q_OBJECT

public:
    struct CustomOption final {
        QString title;
        QString category;
    };

    enum Roles {
        TitleRole = Qt::UserRole + 1,
        SourceIdRole,
        VersionRole,
        DefaultUrlRole,
        AttributionRole,
        LegacyPresetIndexRole,
        BundledRole,
        CustomRole,
        CategoryRole,
    };

    explicit SkyCatalogSourcePresetModel(
        QVector<SkyCatalogSourceDescriptor> descriptors, bool includeCustomOption, QObject* parent = nullptr
    );
    explicit SkyCatalogSourcePresetModel(
        QVector<SkyCatalogSourceDescriptor> descriptors, QVector<CustomOption> customOptions, QObject* parent = nullptr
    );

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] Q_INVOKABLE QString sourceIdAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString titleAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString defaultUrlAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString versionAt(int row) const;
    [[nodiscard]] Q_INVOKABLE QString categoryAt(int row) const;
    [[nodiscard]] Q_INVOKABLE int legacyPresetIndexAt(int row) const;
    [[nodiscard]] Q_INVOKABLE bool isCustomAt(int row) const;

private:
    struct Row final {
        SkyCatalogSourceDescriptor descriptor;
        bool custom = false;
        QString category;
    };

    [[nodiscard]] const Row* rowAt(int row) const;

private:
    QVector<Row> m_rows;
};

}  // namespace skygate::ui::internal
