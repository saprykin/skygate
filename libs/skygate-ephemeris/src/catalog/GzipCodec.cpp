#include "catalog/GzipCodec.hpp"

#include <array>
#include <cstddef>
#include <zlib.h>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kMaxGunzipOutputBytes = 768U << 20;

}  // namespace

std::optional<std::string> GzipCodec::gunzip(const std::string_view gzipData) const
{
    if (gzipData.empty()) {
        return std::nullopt;
    }

    z_stream stream {};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(gzipData.data()));
    stream.avail_in = static_cast<uInt>(gzipData.size());

    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return std::nullopt;
    }

    std::string output;
    output.reserve(gzipData.size() * 3);

    std::array<char, 1 << 14> buffer {};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());

        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            return std::nullopt;
        }

        const std::size_t producedBytes = buffer.size() - stream.avail_out;
        output.append(buffer.data(), producedBytes);
        if (output.size() > kMaxGunzipOutputBytes) {
            inflateEnd(&stream);
            return std::nullopt;
        }
    }

    inflateEnd(&stream);
    if (status != Z_STREAM_END || output.empty()) {
        return std::nullopt;
    }

    return output;
}

}  // namespace skygate::ephemeris
