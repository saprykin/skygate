#include "catalog/io/CompressedCatalogParser.hpp"
#include "catalog/io/CompressedDataInflater.hpp"

#include <QLoggingCategory>
#include <QString>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

CatalogBodyParseResult CompressedCatalogParser::parse(
    const std::string_view compressedData,
    const HygParseProgressCallback& progressCallback,
    const InnerParser& innerParser,
    const std::string& emptyDetail,
    const std::string& decompressDetail
)
{
    CatalogBodyParseResult result;
    if (compressedData.empty()) {
        result.errorCode = CatalogLoadResult::ErrorCode::EmptyInput;
        result.errorDetail = emptyDetail;
        qCWarning(skygateCatalogParseLog).noquote()
            << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    const auto uncompressedData = CompressedDataInflater::inflate(compressedData, CompressedDataInflater::Format::Gzip);
    if (!uncompressedData.has_value()) {
        result.errorCode = CatalogLoadResult::ErrorCode::InvalidGzipData;
        result.errorDetail = decompressDetail;
        qCWarning(skygateCatalogParseLog).noquote()
            << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    return innerParser(*uncompressedData, progressCallback);
}

}  // namespace skygate::ephemeris
