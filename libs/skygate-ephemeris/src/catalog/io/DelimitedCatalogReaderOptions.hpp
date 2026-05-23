#pragma once

#include "catalog/model/CatalogBodyParseResult.hpp"

#include <QChar>
#include <QString>

#include <cstddef>
#include <string>
#include <vector>

namespace skygate::ephemeris {

struct DelimitedCatalogReaderOptions {
    QChar separator = ',';
    std::vector<QString> requiredColumns;
    CatalogLoadErrorCode invalidErrorCode = CatalogLoadErrorCode::UnsupportedFormat;
    std::size_t rowCountLimitFloor = 0;
    std::size_t minExpectedBytesPerDataRow = 1;
    std::string emptyInputDetail;
    std::string missingColumnsDetail;
    std::string rowLimitDetail;
};

}  // namespace skygate::ephemeris
