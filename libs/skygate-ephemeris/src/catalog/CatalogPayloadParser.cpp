#include "CatalogPayloadParser.hpp"
#include "CatalogLoader.hpp"
#include "catalog/CatalogParsingLogging.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "catalog/io/CatalogContainerSignature.hpp"
#include "catalog/io/CatalogPayloadFormatDetector.hpp"
#include "catalog/io/CatalogZipEntrySelector.hpp"
#include "catalog/io/CompressedDataInflater.hpp"

#include <QLoggingCategory>
#include <QString>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

CatalogLoadResult logSuccessfulParse(CatalogLoadResult result)
{
    if (!result.isSuccess()) {
        return result;
    }

    qCInfo(skygateCatalogParseLog).noquote()
        << "Catalog payload parsed: format"
        << QString::fromStdString(CatalogSchemaRegistry::diagnosticName(result.detectedFormat)) << "parsed"
        << static_cast<qulonglong>(result.diagnostics.parsedBodyCount) << "selected"
        << static_cast<qulonglong>(result.diagnostics.selectedBodyCount) << "truncated"
        << static_cast<qulonglong>(result.diagnostics.truncatedBodyCount);
    return result;
}

}  // namespace

CatalogSourceType CatalogPayloadParser::detectFormat(const std::string_view payload) const noexcept
{
    return CatalogPayloadFormatDetector::detect(payload);
}

CatalogLoadResult CatalogPayloadParser::parseResult(const CatalogParseRequest& request) const
{
    CatalogLoadResult result;
    if (request.payload.empty()) {
        result.errorCode = CatalogLoadResult::ErrorCode::EmptyInput;
        result.errorDetail = "Catalog payload is empty.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog payload parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    // Decode at most one archive layer before schema detection. After
    // unwrapping, the payload must be a plain-text schema; a nested archive is
    // reported as an unsupported inner schema rather than decoded recursively.
    std::optional<std::string> unwrappedData;
    std::string_view schemaPayload = request.payload;
    bool decodedContainer = false;

    if (CatalogContainerSignature::isGzip(request.payload)) {
        unwrappedData = CompressedDataInflater::inflate(request.payload, CompressedDataInflater::Format::Gzip);
        if (!unwrappedData.has_value()) {
            result.errorCode = CatalogLoadResult::ErrorCode::InvalidGzipData;
            result.errorDetail = "Gzip catalog payload could not be decompressed.";
            qCWarning(skygateCatalogParseLog).noquote()
                << "Gzip catalog parse failed:" << QString::fromStdString(result.errorDetail);
            return result;
        }
        schemaPayload = *unwrappedData;
        decodedContainer = true;
    } else if (CatalogContainerSignature::isZip(request.payload)) {
        CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(request.payload, request.memberSelector);
        if (selection.status == CatalogZipEntrySelection::Status::Selected) {
            unwrappedData = std::move(selection.payload);
            schemaPayload = *unwrappedData;
            decodedContainer = true;
        } else {
            result.errorCode = selection.errorCode();
            result.errorDetail = selection.errorDetail();
            qCWarning(skygateCatalogParseLog).noquote()
                << "Catalog ZIP parse failed:" << QString::fromStdString(result.errorDetail);
            return result;
        }
    }

    const CatalogSourceType schemaType = CatalogPayloadFormatDetector::detect(schemaPayload);
    if (schemaType == CatalogSourceType::Unknown) {
        result.errorCode = CatalogLoadResult::ErrorCode::UnsupportedFormat;
        result.errorDetail =
            decodedContainer ? "Catalog container decoded, but the inner payload is not a recognized catalog format."
                             : "Catalog payload format is not recognized.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog payload parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    if (request.schemaHint != CatalogSourceType::Unknown && request.schemaHint != schemaType) {
        result.errorCode = CatalogLoadResult::ErrorCode::SchemaHintMismatch;
        result.errorDetail = "Catalog payload schema '" + CatalogSchemaRegistry::diagnosticName(schemaType)
                             + "' does not match the expected schema hint '"
                             + CatalogSchemaRegistry::diagnosticName(request.schemaHint) + "'.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog payload parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    result = CatalogLoader::load(schemaType, schemaPayload, request.progressCallback, request.selectionOptions);
    result.detectedFormat = schemaType;
    return logSuccessfulParse(std::move(result));
}

CatalogLoadResult CatalogPayloadParser::parseResult(
    const std::string_view payload,
    const CatalogParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
) const
{
    return parseResult(
        CatalogParseRequest{
            .payload = payload, .progressCallback = progressCallback, .selectionOptions = selectionOptions
        }
    );
}

std::unique_ptr<IStarCatalog> CatalogPayloadParser::parse(
    const std::string_view payload,
    const CatalogParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
) const
{
    CatalogLoadResult result = parseResult(payload, progressCallback, selectionOptions);
    return std::move(result.catalog);
}

}  // namespace skygate::ephemeris
