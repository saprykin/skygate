#include "catalog/io/GzipCatalogParser.hpp"
#include "catalog/CatalogLoadResult.hpp"
#include "catalog/io/CompressedDataInflater.hpp"

#include <QLoggingCategory>
#include <QString>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

GzipCatalogParser::GzipCatalogParser(std::unique_ptr<ICatalogParser> innerParser)
    : m_innerParser{std::move(innerParser)}
{
}

CatalogBodyParseResult
GzipCatalogParser::parse(const std::string_view data, const CatalogParseProgressCallback& progressCallback) const
{
    CatalogBodyParseResult result;
    if (data.empty()) {
        result.errorCode = CatalogLoadResult::ErrorCode::EmptyInput;
        result.errorDetail = "Gzip catalog payload is empty.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    const auto uncompressedData = CompressedDataInflater::inflate(data, CompressedDataInflater::Format::Gzip);
    if (!uncompressedData.has_value()) {
        result.errorCode = CatalogLoadResult::ErrorCode::InvalidGzipData;
        result.errorDetail = "Gzip catalog payload could not be decompressed.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    return m_innerParser->parse(*uncompressedData, progressCallback);
}

}  // namespace skygate::ephemeris
