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
        const std::string_view line = StringUtilities::trimAsciiWhitespace(payload.substr(cursor, lineEnd - cursor));
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
    return firstByte == 0x50U && secondByte == 0x4bU
           && ((thirdByte == 0x03U && fourthByte == 0x04U) || (thirdByte == 0x05U && fourthByte == 0x06U)
               || (thirdByte == 0x07U && fourthByte == 0x08U));
}

}  // namespace

CatalogSourceType CatalogPayloadFormatDetector::detect(const std::string_view payload) noexcept
{
    if (hasGzipSignature(payload)) {
        return CatalogSourceType::HygCsvGzip;
    }

    if (hasZipSignature(payload)) {
        return CatalogSourceType::HygCsvZip;
    }

    const std::string_view headerLine = firstNonEmptyLine(payload);
    if (headerLine.empty()) {
        return CatalogSourceType::Unknown;
    }

    if (headerLine.find(',') != std::string_view::npos && StringUtilities::containsIgnoreAsciiCase(headerLine, "ra")
        && StringUtilities::containsIgnoreAsciiCase(headerLine, "dec")
        && StringUtilities::containsIgnoreAsciiCase(headerLine, "mag")) {
        return CatalogSourceType::HygCsv;
    }

    if (headerLine.find(';') != std::string_view::npos && StringUtilities::containsIgnoreAsciiCase(headerLine, "Name")
        && StringUtilities::containsIgnoreAsciiCase(headerLine, "Type")
        && StringUtilities::containsIgnoreAsciiCase(headerLine, "RA")
        && StringUtilities::containsIgnoreAsciiCase(headerLine, "Dec")) {
        return CatalogSourceType::OpenNgcCsv;
    }

    return CatalogSourceType::Unknown;
}

}  // namespace skygate::ephemeris
