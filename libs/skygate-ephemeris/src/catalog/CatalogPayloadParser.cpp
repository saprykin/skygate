#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include "catalog/CatalogPayloadFormatDetector.hpp"
#include "catalog/ZipCodec.hpp"

#include <QLoggingCategory>
#include <QString>

#include <string>
#include <utility>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

QString catalogPayloadFormatText(const CatalogPayloadFormat format)
{
    switch (format) {
    case CatalogPayloadFormat::HygCsv:
        return QStringLiteral("HYG CSV");
    case CatalogPayloadFormat::HygCsvGzip:
        return QStringLiteral("HYG CSV gzip");
    case CatalogPayloadFormat::HygCsvZip:
        return QStringLiteral("HYG CSV ZIP");
    case CatalogPayloadFormat::OpenNgcCsv:
        return QStringLiteral("OpenNGC CSV");
    case CatalogPayloadFormat::Unknown:
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
        << "Catalog payload parsed: format" << catalogPayloadFormatText(result.detectedFormat)
        << "parsed" << static_cast<qulonglong>(result.diagnostics.parsedBodyCount)
        << "selected" << static_cast<qulonglong>(result.diagnostics.selectedBodyCount)
        << "truncated" << static_cast<qulonglong>(result.diagnostics.truncatedBodyCount);
    return result;
}

}  // namespace

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
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog payload parse failed:" << QString::fromStdString(result.errorDetail);
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
        return logSuccessfulParse(std::move(result));
    case CatalogPayloadFormat::HygCsvGzip:
        result = loadStarCatalog(
            CatalogSourceType::HygCsvGzip,
            request.payload,
            request.progressCallback,
            request.selectionOptions
        );
        result.detectedFormat = CatalogPayloadFormat::HygCsvGzip;
        return logSuccessfulParse(std::move(result));
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
            return logSuccessfulParse(std::move(result));
        }
        result.errorCode = CatalogLoadErrorCode::InvalidZipData;
        result.errorDetail = "ZIP catalog payload does not contain a readable CSV entry.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog ZIP parse failed:" << QString::fromStdString(result.errorDetail);
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
        return logSuccessfulParse(std::move(result));
    case CatalogPayloadFormat::Unknown:
        break;
    }

    result.errorCode = CatalogLoadErrorCode::UnsupportedFormat;
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
