#include "catalog/ZipCodec.hpp"

#include "catalog/ZipArchiveReader.hpp"

namespace skygate::ephemeris {

std::optional<std::string> ZipCodec::extractFirstCsvEntry(const std::string_view zipData) const
{
    return ZipArchiveReader::extractFirstCsvEntry(zipData);
}

}  // namespace skygate::ephemeris
