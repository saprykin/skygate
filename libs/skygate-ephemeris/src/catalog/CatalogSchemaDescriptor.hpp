#pragma once

#include "CatalogSourceType.hpp"
#include "ICatalogParser.hpp"

#include <QChar>
#include <QString>

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
};

}  // namespace skygate::ephemeris
