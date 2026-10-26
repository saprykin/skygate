#include "CatalogPayloadFormatDetector.hpp"
#include "CatalogHeaderPolicy.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"

#include <QHash>
#include <QString>

#include <string_view>

namespace skygate::ephemeris {

CatalogSourceType CatalogPayloadFormatDetector::detect(const std::string_view payload) noexcept
{
    std::string_view remaining = payload;
    const std::string_view headerLine = CatalogHeaderPolicy::findHeaderLine(remaining);
    if (headerLine.empty()) {
        return CatalogSourceType::Unknown;
    }

    // Schemas without required header columns (for example the bundled data)
    // cannot be identified by their header and are skipped here.
    for (const CatalogSchemaDescriptor& descriptor : CatalogSchemaRegistry::descriptors()) {
        if (descriptor.requiredColumns.empty()) {
            continue;
        }

        const QHash<QString, qsizetype> header =
            CatalogHeaderPolicy::decodeHeaderLine(headerLine, descriptor.delimiter);
        if (CatalogHeaderPolicy::hasRequiredColumns(header, descriptor.requiredColumns)) {
            return descriptor.type;
        }
    }

    return CatalogSourceType::Unknown;
}

}  // namespace skygate::ephemeris
