#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include "catalog/CatalogPayloadFormatDetector.hpp"
#include "catalog/ZipCodec.hpp"

#include <string>

namespace skygate::ephemeris {

CatalogPayloadFormat CatalogPayloadParser::detectFormat(const std::string_view payload) const noexcept
{
    return CatalogPayloadFormatDetector::detect(payload);
}

CatalogLoadResult CatalogPayloadParser::parseResult(const CatalogParseRequest& request) const
{
    CatalogLoadResult result;
    if (request.payload.empty()) {
        result.errorCode = CatalogLoadErrorCode::EmptyInput;
        result.errorDetail = "Catalog payload is empty.";
        return result;
    }

    result.detectedFormat = detectFormat(request.payload);
    switch (result.detectedFormat) {
    case CatalogPayloadFormat::HygCsv:
        result = loadStarCatalog(
            CatalogSourceType::HygCsv,
            request.payload,
            request.progressCallback,
            request.selectionOptions
        );
        result.detectedFormat = CatalogPayloadFormat::HygCsv;
        return result;
    case CatalogPayloadFormat::HygCsvGzip:
        result = loadStarCatalog(
            CatalogSourceType::HygCsvGzip,
            request.payload,
            request.progressCallback,
            request.selectionOptions
        );
        result.detectedFormat = CatalogPayloadFormat::HygCsvGzip;
        return result;
    case CatalogPayloadFormat::HygCsvZip: {
        const ZipCodec zipCodec;
        const auto extractedCsv = zipCodec.extractFirstCsvEntry(request.payload);
        if (extractedCsv.has_value()) {
            result = loadStarCatalog(
                CatalogSourceType::HygCsv,
                *extractedCsv,
                request.progressCallback,
                request.selectionOptions
            );
            result.detectedFormat = CatalogPayloadFormat::HygCsvZip;
            return result;
        }
        result.errorCode = CatalogLoadErrorCode::InvalidZipData;
        result.errorDetail = "ZIP catalog payload does not contain a readable CSV entry.";
        return result;
    }
    case CatalogPayloadFormat::OpenNgcCsv:
        result = loadStarCatalog(
            CatalogSourceType::OpenNgcCsv,
            request.payload,
            request.progressCallback,
            request.selectionOptions
        );
        result.detectedFormat = CatalogPayloadFormat::OpenNgcCsv;
        return result;
    case CatalogPayloadFormat::Unknown:
        break;
    }

    result.errorCode = CatalogLoadErrorCode::UnsupportedFormat;
    result.errorDetail = "Catalog payload format is not recognized.";
    return result;
}

CatalogLoadResult CatalogPayloadParser::parseResult(
    const std::string_view payload,
    const HygParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
) const
{
    return parseResult(
        CatalogParseRequest {
            .payload = payload,
            .progressCallback = progressCallback,
            .selectionOptions = selectionOptions
        }
    );
}

std::unique_ptr<IStarCatalog> CatalogPayloadParser::parse(
    const std::string_view payload,
    const HygParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
) const
{
    CatalogLoadResult result = parseResult(payload, progressCallback, selectionOptions);
    return std::move(result.catalog);
}

}  // namespace skygate::ephemeris
