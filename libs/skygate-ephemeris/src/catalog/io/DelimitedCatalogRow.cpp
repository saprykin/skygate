#include "DelimitedCatalogRow.hpp"
#include "CsvRowTokenizer.hpp"

#include <utility>

namespace skygate::ephemeris {

DelimitedCatalogRow::DelimitedCatalogRow(
    QVector<QStringView> columns, const QHash<QString, qsizetype>& headerIndex
) noexcept
    : m_columns(std::move(columns)), m_headerIndex(headerIndex)
{
}

QString DelimitedCatalogRow::decodeColumn(const QString& name) const
{
    return decodeColumn(columnIndex(name));
}

QString DelimitedCatalogRow::decodeColumn(const qsizetype columnIndex) const
{
    if (columnIndex < 0 || columnIndex >= m_columns.size()) {
        return {};
    }

    return CsvRowTokenizer::decodeField(m_columns.at(columnIndex)).trimmed();
}

qsizetype DelimitedCatalogRow::columnIndex(const QString& name) const
{
    const auto it = m_headerIndex.constFind(name.toLower());
    if (it == m_headerIndex.cend()) {
        return -1;
    }
    return *it;
}

}  // namespace skygate::ephemeris
