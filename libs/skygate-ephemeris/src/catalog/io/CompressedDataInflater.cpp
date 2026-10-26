#include "CompressedDataInflater.hpp"

#include <array>
#include <limits>
#include <zlib.h>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kMaxInflatedCatalogBytes = 768U << 20;

int windowBitsForFormat(const CompressedDataInflater::Format format)
{
    switch (format) {
    case CompressedDataInflater::Format::Gzip:
        return 16 + MAX_WBITS;
    case CompressedDataInflater::Format::RawDeflate:
        return -MAX_WBITS;
    }

    return MAX_WBITS;
}

}  // namespace

std::optional<std::string> CompressedDataInflater::inflate(
    const std::string_view compressedData, const Format format, const CompressedDataInflateOptions& options
)
{
    if (compressedData.empty()) {
        return std::nullopt;
    }
    if (compressedData.size() > std::numeric_limits<uInt>::max()) {
        return std::nullopt;
    }
    if (options.expectedOutputBytes > kMaxInflatedCatalogBytes) {
        return std::nullopt;
    }

    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.data()));
    stream.avail_in = static_cast<uInt>(compressedData.size());

    if (inflateInit2(&stream, windowBitsForFormat(format)) != Z_OK) {
        return std::nullopt;
    }

    std::string output;
    if (options.expectedOutputBytes > 0U) {
        output.reserve(options.expectedOutputBytes);
    } else {
        output.reserve(compressedData.size() * 3U);
    }

    std::array<char, 1 << 14> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());

        status = ::inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            return std::nullopt;
        }

        const std::size_t producedBytes = buffer.size() - stream.avail_out;
        output.append(buffer.data(), producedBytes);
        if (output.size() > kMaxInflatedCatalogBytes) {
            inflateEnd(&stream);
            return std::nullopt;
        }
    }

    inflateEnd(&stream);
    if (status != Z_STREAM_END || (!options.allowEmptyOutput && output.empty())) {
        return std::nullopt;
    }
    if (options.expectedOutputBytes > 0U && output.size() != options.expectedOutputBytes) {
        return std::nullopt;
    }

    return output;
}

}  // namespace skygate::ephemeris
