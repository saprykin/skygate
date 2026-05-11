#pragma once

#include "catalog/model/CatalogBodyParseResult.hpp"

#include <QChar>
#include <QHash>
#include <QString>
#include <QStringView>
#include <QVector>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

struct DelimitedCatalogReaderOptions {
    QChar separator = ',';
    std::vector<QString> requiredColumns;
    CatalogLoadErrorCode invalidErrorCode = CatalogLoadErrorCode::UnsupportedFormat;
    std::size_t minRowCountLimit = 0;
    std::size_t minPlausibleRowBytes = 1;
    std::string emptyInputDetail;
    std::string missingColumnsDetail;
    std::string rowLimitDetail;
};

class DelimitedCatalogReader final {
public:
    class Row final {
    public:
        Row(
            QVector<QStringView> columns,
            const QHash<QString, qsizetype>& headerIndex
        ) noexcept;

        [[nodiscard]] QString decodeColumn(const QString& name) const;
        [[nodiscard]] QString decodeColumn(qsizetype columnIndex) const;
        [[nodiscard]] qsizetype columnIndex(const QString& name) const;

    private:
        QVector<QStringView> m_columns;
        const QHash<QString, qsizetype>& m_headerIndex;
    };

    using RowHandler = std::function<bool(const Row& row, CatalogBodyParseResult& result)>;

    [[nodiscard]] static CatalogBodyParseResult read(
        std::string_view payload,
        const DelimitedCatalogReaderOptions& options,
        const RowHandler& rowHandler
    );
};

}  // namespace skygate::ephemeris
