#include "ZipCatalogParser.hpp"
#include "ZipCodec.hpp"
#include "catalog/CatalogLoadResult.hpp"
#include "catalog/hyg/HygCatalogParser.hpp"

#include <QLoggingCategory>
#include <QString>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

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

    const HygCatalogParser hygParser;
    return hygParser.parse(*extractedCsv, progressCallback);
}

}  // namespace skygate::ephemeris
