#include "CatalogPayloadParser.hpp"
#include "CatalogLoader.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
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

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

bool hasGzipSignature(const std::string_view payload) noexcept
{
    if (payload.size() < 2U) {
        return false;
    }

    const auto firstByte = static_cast<unsigned char>(payload[0]);
    const auto secondByte = static_cast<unsigned char>(payload[1]);
    return firstByte == 0x1fU && secondByte == 0x8bU;
}

bool hasZipSignature(const std::string_view payload) noexcept
{
    if (payload.size() < 4U) {
        return false;
    }

    const auto firstByte = static_cast<unsigned char>(payload[0]);
    const auto secondByte = static_cast<unsigned char>(payload[1]);
    const auto thirdByte = static_cast<unsigned char>(payload[2]);
    const auto fourthByte = static_cast<unsigned char>(payload[3]);
    return firstByte == 0x50U && secondByte == 0x4bU
           && ((thirdByte == 0x03U && fourthByte == 0x04U) || (thirdByte == 0x05U && fourthByte == 0x06U)
               || (thirdByte == 0x07U && fourthByte == 0x08U));
}

QString catalogSourceTypeText(const CatalogSourceType type)
{
    return QString::fromStdString(CatalogSchemaRegistry::diagnosticName(type));
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

    // Decode at most one archive layer before schema detection. After
    // unwrapping, the payload must be a plain-text schema; a nested archive is
    // reported as an unsupported inner schema rather than decoded recursively.
    std::optional<std::string> unwrappedData;
    std::string_view schemaPayload = request.payload;
    bool decodedContainer = false;

    if (hasGzipSignature(request.payload)) {
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
    } else if (hasZipSignature(request.payload)) {
        CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(request.payload, request.memberSelector);
        if (selection.status == CatalogZipEntrySelection::Status::Selected) {
            unwrappedData = std::move(selection.payload);
            schemaPayload = *unwrappedData;
            decodedContainer = true;
        } else {
            switch (selection.status) {
            case CatalogZipEntrySelection::Status::Selected:
                break;
            case CatalogZipEntrySelection::Status::InvalidArchive:
                result.errorCode = CatalogLoadResult::ErrorCode::InvalidZipData;
                result.errorDetail = "ZIP catalog payload could not be parsed.";
                break;
            case CatalogZipEntrySelection::Status::MissingMember:
                result.errorCode = CatalogLoadResult::ErrorCode::ArchiveMemberNotFound;
                result.errorDetail = "ZIP catalog payload does not contain member '" + selection.selectedPath + "'.";
                break;
            case CatalogZipEntrySelection::Status::UnusableMember:
                result.errorCode = CatalogLoadResult::ErrorCode::InvalidZipData;
                result.errorDetail =
                    "ZIP catalog member '" + selection.selectedPath + "' is not a readable catalog entry.";
                break;
            case CatalogZipEntrySelection::Status::AmbiguousMember:
                result.errorCode = CatalogLoadResult::ErrorCode::AmbiguousArchiveMember;
                result.errorDetail = "ZIP catalog payload contains multiple supported catalog members.";
                break;
            case CatalogZipEntrySelection::Status::NoSupportedMember:
                result.errorCode = CatalogLoadResult::ErrorCode::InvalidZipData;
                result.errorDetail = "ZIP catalog payload does not contain a supported catalog member.";
                break;
            case CatalogZipEntrySelection::Status::NoReadableEntry:
                result.errorCode = CatalogLoadResult::ErrorCode::InvalidZipData;
                result.errorDetail = "ZIP catalog payload does not contain a readable CSV entry.";
                break;
            }
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
