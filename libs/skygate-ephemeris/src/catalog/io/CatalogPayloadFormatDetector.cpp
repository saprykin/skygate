#include "catalog/io/CatalogPayloadFormatDetector.hpp"

#include "StringUtilities.hpp"

#include <cstddef>

namespace skygate::ephemeris {
namespace {

std::string_view firstNonEmptyLine(std::string_view payload)
{
    std::size_t cursor = 0;
    while (cursor < payload.size()) {
        const std::size_t newline = payload.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? payload.size() : newline;
        const std::string_view line = strings::trimAsciiWhitespace(payload.substr(cursor, lineEnd - cursor));
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
    return firstByte == 0x50U
        && secondByte == 0x4bU
        && (
            (thirdByte == 0x03U && fourthByte == 0x04U)
            || (thirdByte == 0x05U && fourthByte == 0x06U)
            || (thirdByte == 0x07U && fourthByte == 0x08U)
        );
}

}  // namespace

CatalogPayloadFormat CatalogPayloadFormatDetector::detect(const std::string_view payload) noexcept
{
    if (hasGzipSignature(payload)) {
        return CatalogPayloadFormat::HygCsvGzip;
    }

    if (hasZipSignature(payload)) {
        return CatalogPayloadFormat::HygCsvZip;
    }

    const std::string_view headerLine = firstNonEmptyLine(payload);
    if (headerLine.empty()) {
        return CatalogPayloadFormat::Unknown;
    }

    if (
        headerLine.find(',') != std::string_view::npos
        && strings::containsIgnoreAsciiCase(headerLine, "ra")
        && strings::containsIgnoreAsciiCase(headerLine, "dec")
        && strings::containsIgnoreAsciiCase(headerLine, "mag")
    ) {
        return CatalogPayloadFormat::HygCsv;
    }

    if (
        headerLine.find(';') != std::string_view::npos
        && strings::containsIgnoreAsciiCase(headerLine, "Name")
        && strings::containsIgnoreAsciiCase(headerLine, "Type")
        && strings::containsIgnoreAsciiCase(headerLine, "RA")
        && strings::containsIgnoreAsciiCase(headerLine, "Dec")
    ) {
        return CatalogPayloadFormat::OpenNgcCsv;
    }

    return CatalogPayloadFormat::Unknown;
}

}  // namespace skygate::ephemeris
