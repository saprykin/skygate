#pragma once

#include "catalog/CatalogBodyParseResult.hpp"

#include <QHash>
#include <QString>
#include <QStringView>
#include <QVector>

#include <QtGlobal>

#include <functional>

namespace skygate::ephemeris {

class DelimitedCatalogRow final {
public:
    DelimitedCatalogRow(QVector<QStringView> columns, const QHash<QString, qsizetype>& headerIndex) noexcept;

    [[nodiscard]] QString decodeColumn(const QString& name) const;
    [[nodiscard]] QString decodeColumn(qsizetype columnIndex) const;
    [[nodiscard]] qsizetype columnIndex(const QString& name) const;

private:
    QVector<QStringView> m_columns;
    const QHash<QString, qsizetype>& m_headerIndex;
};

using DelimitedCatalogRowHandler = std::function<bool(const DelimitedCatalogRow& row, CatalogBodyParseResult& result)>;

}  // namespace skygate::ephemeris
