#include "catalog/HygGzipCatalogParser.hpp"

#include "catalog/GzipCodec.hpp"
#include "catalog/HygCatalogParser.hpp"

#include <QLoggingCategory>
#include <QString>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

CatalogBodyParseResult HygGzipCatalogParser::parse(
    const std::string_view gzipData,
    const HygParseProgressCallback& progressCallback
) const
{
    CatalogBodyParseResult result;
    if (gzipData.empty()) {
        result.errorCode = CatalogLoadErrorCode::EmptyInput;
        result.errorDetail = "Gzip catalog payload is empty.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    const GzipCodec gzipCodec;
    const auto uncompressedData = gzipCodec.gunzip(gzipData);
    if (!uncompressedData.has_value()) {
        result.errorCode = CatalogLoadErrorCode::InvalidGzipData;
        result.errorDetail = "Gzip catalog payload could not be decompressed.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    const HygCatalogParser hygParser;
    return hygParser.parse(*uncompressedData, progressCallback);
}

}  // namespace skygate::ephemeris
