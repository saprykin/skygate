#include "CatalogPayloadFormatDetector.hpp"
#include "CatalogHeaderPolicy.hpp"

#include <QHash>
#include <QString>

#include <vector>

namespace skygate::ephemeris {
namespace {

const std::vector<QString> kHygRequiredColumns = {
    QStringLiteral("ra"),
    QStringLiteral("dec"),
    QStringLiteral("mag"),
};

const std::vector<QString> kOpenNgcRequiredColumns = {
    QStringLiteral("Name"),
    QStringLiteral("Type"),
    QStringLiteral("RA"),
    QStringLiteral("Dec"),
};

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

    const QHash<QString, qsizetype> commaHeader = CatalogHeaderPolicy::decodeHeaderLine(headerLine, QChar{','});
    if (CatalogHeaderPolicy::hasRequiredColumns(commaHeader, kHygRequiredColumns)) {
        return CatalogSourceType::HygCsv;
    }

    const QHash<QString, qsizetype> semicolonHeader = CatalogHeaderPolicy::decodeHeaderLine(headerLine, QChar{';'});
    if (CatalogHeaderPolicy::hasRequiredColumns(semicolonHeader, kOpenNgcRequiredColumns)) {
        return CatalogSourceType::OpenNgcCsv;
    }

    return CatalogSourceType::Unknown;
}

}  // namespace skygate::ephemeris
