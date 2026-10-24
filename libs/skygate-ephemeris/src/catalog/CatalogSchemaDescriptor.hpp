#pragma once

#include "CatalogSourceType.hpp"
#include "ICatalogParser.hpp"

#include <QChar>
#include <QString>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace skygate::ephemeris {

using CatalogParserFactory = std::function<std::unique_ptr<ICatalogParser>()>;

// Describes one supported catalog schema. The delimiter, exact required column
// names, and diagnostic name are shared by header detection, parser selection,
// and parse diagnostics so adding a schema does not require independent switch
// updates. The factory constructs the parser adapter for the schema.
struct CatalogSchemaDescriptor final {
    CatalogSourceType type = CatalogSourceType::Unknown;
    std::string diagnosticName;
    QChar delimiter = ',';
    std::vector<QString> requiredColumns;
    CatalogParserFactory createParser;

    // Reader diagnostics of a delimited schema. Each names the schema by its
    // diagnostic name and, for a missing column, lists the required columns of
    // this descriptor, so the messages and the header probe cannot describe
    // different column sets.
    [[nodiscard]] std::string emptyInputDetail() const
    {
        return diagnosticName + " payload is empty.";
    }

    [[nodiscard]] std::string missingRequiredColumnsDetail() const
    {
        std::string detail = diagnosticName + " payload is missing one of the required columns: ";
        for (std::size_t index = 0; index < requiredColumns.size(); ++index) {
            if (index > 0U) {
                detail += ", ";
            }
            detail += requiredColumns[index].toStdString();
        }
        return detail + ".";
    }

    [[nodiscard]] std::string rowLimitDetail() const
    {
        return diagnosticName + " payload exceeds the supported row limit.";
    }
};

}  // namespace skygate::ephemeris
