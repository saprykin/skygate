#include "catalog/CatalogPayloadParser.hpp"

#include "catalog/CatalogLoader.hpp"

#include "catalog/io/CatalogPayloadFormatDetector.hpp"
#include "catalog/io/zip/ZipCodec.hpp"

#include <QLoggingCategory>
#include <QString>

#include <string>
#include <utility>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

QString catalogPayloadFormatText(const CatalogLoadResult::PayloadFormat format)
{
    switch (format) {
    case CatalogLoadResult::PayloadFormat::HygCsv:
        return QStringLiteral("HYG CSV");
    case CatalogLoadResult::PayloadFormat::HygCsvGzip:
        return QStringLiteral("HYG CSV gzip");
    case CatalogLoadResult::PayloadFormat::HygCsvZip:
        return QStringLiteral("HYG CSV ZIP");
    case CatalogLoadResult::PayloadFormat::OpenNgcCsv:
        return QStringLiteral("OpenNGC CSV");
    case CatalogLoadResult::PayloadFormat::Unknown:
        return QStringLiteral("unknown");
    }

    return QStringLiteral("unknown");
}

CatalogLoadResult logSuccessfulParse(CatalogLoadResult result)
{
    if (!result.isSuccess()) {
        return result;
    }

    qCInfo(skygateCatalogParseLog).noquote()
        << "Catalog payload parsed: format" << catalogPayloadFormatText(result.detectedFormat) << "parsed"
        << static_cast<qulonglong>(result.diagnostics.parsedBodyCount) << "selected"
        << static_cast<qulonglong>(result.diagnostics.selectedBodyCount) << "truncated"
        << static_cast<qulonglong>(result.diagnostics.truncatedBodyCount);
    return result;
}

}  // namespace

CatalogLoadResult::PayloadFormat CatalogPayloadParser::detectFormat(const std::string_view payload) const noexcept
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

    result.detectedFormat = detectFormat(request.payload);
    switch (result.detectedFormat) {
    case CatalogLoadResult::PayloadFormat::HygCsv:
        result = CatalogLoader::load(
            CatalogSourceType::HygCsv, request.payload, request.progressCallback, request.selectionOptions
        );
        result.detectedFormat = CatalogLoadResult::PayloadFormat::HygCsv;
        return logSuccessfulParse(std::move(result));
    case CatalogLoadResult::PayloadFormat::HygCsvGzip:
        result = CatalogLoader::load(
            CatalogSourceType::HygCsvGzip, request.payload, request.progressCallback, request.selectionOptions
        );
        result.detectedFormat = CatalogLoadResult::PayloadFormat::HygCsvGzip;
        return logSuccessfulParse(std::move(result));
    case CatalogLoadResult::PayloadFormat::HygCsvZip: {
        const ZipCodec zipCodec;
        const auto extractedCsv = zipCodec.extractFirstCsvEntry(request.payload);
        if (extractedCsv.has_value()) {
            result = CatalogLoader::load(
                CatalogSourceType::HygCsv, *extractedCsv, request.progressCallback, request.selectionOptions
            );
            result.detectedFormat = CatalogLoadResult::PayloadFormat::HygCsvZip;
            return logSuccessfulParse(std::move(result));
        }
        result.errorCode = CatalogLoadResult::ErrorCode::InvalidZipData;
        result.errorDetail = "ZIP catalog payload does not contain a readable CSV entry.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog ZIP parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }
    case CatalogLoadResult::PayloadFormat::OpenNgcCsv:
        result = CatalogLoader::load(
            CatalogSourceType::OpenNgcCsv, request.payload, request.progressCallback, request.selectionOptions
        );
        result.detectedFormat = CatalogLoadResult::PayloadFormat::OpenNgcCsv;
        return logSuccessfulParse(std::move(result));
    case CatalogLoadResult::PayloadFormat::Unknown:
        break;
    }

    result.errorCode = CatalogLoadResult::ErrorCode::UnsupportedFormat;
    result.errorDetail = "Catalog payload format is not recognized.";
    qCWarning(skygateCatalogParseLog).noquote()
        << "Catalog payload parse failed:" << QString::fromStdString(result.errorDetail);
    return result;
}

CatalogLoadResult CatalogPayloadParser::parseResult(
    const std::string_view payload,
    const HygParseProgressCallback& progressCallback,
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
    const HygParseProgressCallback& progressCallback,
    const CatalogSelectionOptions& selectionOptions
) const
{
    CatalogLoadResult result = parseResult(payload, progressCallback, selectionOptions);
    return std::move(result.catalog);
}

}  // namespace skygate::ephemeris
