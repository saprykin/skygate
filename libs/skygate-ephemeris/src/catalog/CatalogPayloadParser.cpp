#include "CatalogPayloadParser.hpp"
#include "CatalogLoader.hpp"
#include "catalog/io/CatalogPayloadFormatDetector.hpp"

#include <QLoggingCategory>
#include <QString>

#include <string>
#include <utility>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

QString catalogSourceTypeText(const CatalogSourceType type)
{
    switch (type) {
    case CatalogSourceType::HygCsv:
        return QStringLiteral("HYG CSV");
    case CatalogSourceType::HygCsvGzip:
        return QStringLiteral("HYG CSV gzip");
    case CatalogSourceType::HygCsvZip:
        return QStringLiteral("HYG CSV ZIP");
    case CatalogSourceType::OpenNgcCsv:
        return QStringLiteral("OpenNGC CSV");
    case CatalogSourceType::Bundled:
        return QStringLiteral("bundled");
    case CatalogSourceType::Unknown:
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
        << "Catalog payload parsed: format" << catalogSourceTypeText(result.detectedFormat) << "parsed"
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

    const CatalogSourceType detectedType = detectFormat(request.payload);
    if (detectedType == CatalogSourceType::Unknown) {
        result.errorCode = CatalogLoadResult::ErrorCode::UnsupportedFormat;
        result.errorDetail = "Catalog payload format is not recognized.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "Catalog payload parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    result = CatalogLoader::load(detectedType, request.payload, request.progressCallback, request.selectionOptions);
    result.detectedFormat = detectedType;
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
