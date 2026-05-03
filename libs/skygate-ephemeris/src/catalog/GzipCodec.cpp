#include "catalog/GzipCodec.hpp"

#include "catalog/CompressedDataInflater.hpp"

namespace skygate::ephemeris {

std::optional<std::string> GzipCodec::gunzip(const std::string_view gzipData) const
{
    return CompressedDataInflater::inflate(gzipData, CompressedDataFormat::Gzip);
}

}  // namespace skygate::ephemeris
