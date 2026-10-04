#include "CatalogPayloadFormatDetector.hpp"
#include "CatalogHeaderPolicy.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"

#include <QHash>
#include <QString>

#include <string_view>

namespace skygate::ephemeris {
namespace {

bool hasGzipSignature(const std::string_view payload) noexcept
{
    if (payload.size() < 2U) {
        return false;
    }

    const auto firstByte = static_cast<unsigned char>(payload[0]);
    const auto secondByte = static_cast<unsigned char>(payload[1]);
    return firstByte == 0x1fU && secondByte == 0x8bU;
}

bool hasZipSignature(const std::string_view payload) noexcept
{
    if (payload.size() < 4U) {
        return false;
    }

    const auto firstByte = static_cast<unsigned char>(payload[0]);
    const auto secondByte = static_cast<unsigned char>(payload[1]);
    const auto thirdByte = static_cast<unsigned char>(payload[2]);
    const auto fourthByte = static_cast<unsigned char>(payload[3]);
    return firstByte == 0x50U && secondByte == 0x4bU
           && ((thirdByte == 0x03U && fourthByte == 0x04U) || (thirdByte == 0x05U && fourthByte == 0x06U)
               || (thirdByte == 0x07U && fourthByte == 0x08U));
}

}  // namespace

CatalogSourceType CatalogPayloadFormatDetector::detect(const std::string_view payload) noexcept
{
    if (hasGzipSignature(payload)) {
        return CatalogSourceType::HygCsvGzip;
    }

    if (hasZipSignature(payload)) {
        return CatalogSourceType::HygCsvZip;
    }

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
