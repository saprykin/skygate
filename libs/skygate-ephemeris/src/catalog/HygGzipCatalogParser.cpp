#include "catalog/HygGzipCatalogParser.hpp"

#include "catalog/GzipCodec.hpp"
#include "catalog/HygCatalogParser.hpp"

namespace skygate::ephemeris {

std::vector<CelestialBody> HygGzipCatalogParser::parse(
    const std::string_view gzipData,
    const HygParseProgressCallback& progressCallback
) const
{
    const GzipCodec gzipCodec;
    const auto uncompressedData = gzipCodec.gunzip(gzipData);
    if (!uncompressedData.has_value()) {
        return {};
    }

    const HygCatalogParser hygParser;
    return hygParser.parse(*uncompressedData, progressCallback);
}

}  // namespace skygate::ephemeris
