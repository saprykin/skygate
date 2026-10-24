#include "ZipEntryExtractor.hpp"
#include "ZipFieldReader.hpp"
#include "catalog/io/CatalogContainerSignature.hpp"
#include "catalog/io/CompressedDataInflater.hpp"

#include <cstddef>
#include <cstdint>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kZipLocalFileHeaderSize = 30U;

}  // namespace

std::optional<std::string> ZipEntryExtractor::extract(const std::string_view zipData, const ZipEntryMetadata& entry)
{
    if (entry.localHeaderOffset + kZipLocalFileHeaderSize > zipData.size()) {
        return std::nullopt;
    }

    ZipFieldReader fields(zipData);
    if (fields.le32(entry.localHeaderOffset) != CatalogContainerSignature::kZipLocalFileHeaderSignature) {
        return std::nullopt;
    }
    const std::uint16_t fileNameLength = fields.le16(entry.localHeaderOffset + 26U);
    const std::uint16_t extraFieldLength = fields.le16(entry.localHeaderOffset + 28U);
    if (!fields.isComplete()) {
        return std::nullopt;
    }

    const std::size_t fileDataOffset = entry.localHeaderOffset + kZipLocalFileHeaderSize
                                       + static_cast<std::size_t>(fileNameLength)
                                       + static_cast<std::size_t>(extraFieldLength);
    if (fileDataOffset > zipData.size() || entry.compressedSize > (zipData.size() - fileDataOffset)) {
        return std::nullopt;
    }

    const std::string_view compressedData = zipData.substr(fileDataOffset, entry.compressedSize);
    if (entry.compressionMethod == 0U) {
        if (entry.uncompressedSize > 0U && compressedData.size() != entry.uncompressedSize) {
            return std::nullopt;
        }
        if (compressedData.empty()) {
            return std::nullopt;
        }
        return std::string(compressedData);
    }

    if (entry.compressionMethod == 8U) {
        return CompressedDataInflater::inflate(
            compressedData,
            CompressedDataInflater::Format::RawDeflate,
            CompressedDataInflateOptions{.expectedOutputBytes = entry.uncompressedSize, .allowEmptyOutput = false}
        );
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
