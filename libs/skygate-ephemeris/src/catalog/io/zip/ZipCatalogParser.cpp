#include "ZipCatalogParser.hpp"
#include "ZipCodec.hpp"
#include "catalog/CatalogLoadResult.hpp"

#include <QLoggingCategory>
#include <QString>

#include <utility>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

ZipCatalogParser::ZipCatalogParser(std::unique_ptr<ICatalogParser> innerParser) : m_innerParser{std::move(innerParser)}
{
}

CatalogBodyParseResult
ZipCatalogParser::parse(const std::string_view data, const CatalogParseProgressCallback& progressCallback) const
{
    CatalogBodyParseResult result;
    const ZipCodec zipCodec;
    const auto extractedCsv = zipCodec.extractFirstCsvEntry(data);
    if (!extractedCsv.has_value()) {
        result.errorCode = CatalogLoadResult::ErrorCode::InvalidZipData;
        result.errorDetail = "ZIP catalog payload does not contain a readable CSV entry.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog ZIP parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    return m_innerParser->parse(*extractedCsv, progressCallback);
}

}  // namespace skygate::ephemeris
