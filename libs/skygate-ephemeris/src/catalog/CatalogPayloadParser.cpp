#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include "catalog/ZipCodec.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace skygate::ephemeris {
namespace {

std::string toLowerAscii(const std::string_view value)
{
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

bool containsCaseInsensitiveToken(const std::string_view value, const std::string_view token)
{
    const std::string loweredValue = toLowerAscii(value);
    const std::string loweredToken = toLowerAscii(token);
    return loweredValue.find(loweredToken) != std::string::npos;
}

std::string_view trimAsciiWhitespace(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }

    return value;
}

std::string_view firstNonEmptyLine(std::string_view payload)
{
    std::size_t cursor = 0;
    while (cursor < payload.size()) {
        const std::size_t newline = payload.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? payload.size() : newline;
        const std::string_view line = trimAsciiWhitespace(payload.substr(cursor, lineEnd - cursor));
        if (!line.empty() && line.front() != '#') {
            return line;
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }

    return {};
}

}  // namespace

CatalogPayloadFormat CatalogPayloadParser::detectFormat(const std::string_view payload) const noexcept
{
    if (payload.size() >= 2) {
        const auto firstByte = static_cast<unsigned char>(payload[0]);
        const auto secondByte = static_cast<unsigned char>(payload[1]);
        if (firstByte == 0x1fU && secondByte == 0x8bU) {
            return CatalogPayloadFormat::HygCsvGzip;
        }
    }

    if (payload.size() >= 4) {
        const auto firstByte = static_cast<unsigned char>(payload[0]);
        const auto secondByte = static_cast<unsigned char>(payload[1]);
        const auto thirdByte = static_cast<unsigned char>(payload[2]);
        const auto fourthByte = static_cast<unsigned char>(payload[3]);
        const bool isZipSignature = firstByte == 0x50U
            && secondByte == 0x4bU
            && (
                (thirdByte == 0x03U && fourthByte == 0x04U)
                || (thirdByte == 0x05U && fourthByte == 0x06U)
                || (thirdByte == 0x07U && fourthByte == 0x08U)
            );
        if (isZipSignature) {
            return CatalogPayloadFormat::HygCsvZip;
        }
    }

    const std::string_view headerLine = firstNonEmptyLine(payload);
    if (headerLine.empty()) {
        return CatalogPayloadFormat::Unknown;
    }

    if (
        headerLine.find(',') != std::string_view::npos
        && containsCaseInsensitiveToken(headerLine, "ra")
        && containsCaseInsensitiveToken(headerLine, "dec")
        && containsCaseInsensitiveToken(headerLine, "mag")
    ) {
        return CatalogPayloadFormat::HygCsv;
    }

    if (
        headerLine.find(';') != std::string_view::npos
        && containsCaseInsensitiveToken(headerLine, "Name")
        && containsCaseInsensitiveToken(headerLine, "Type")
        && containsCaseInsensitiveToken(headerLine, "RA")
        && containsCaseInsensitiveToken(headerLine, "Dec")
    ) {
        return CatalogPayloadFormat::OpenNgcCsv;
    }

    return CatalogPayloadFormat::Unknown;
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
