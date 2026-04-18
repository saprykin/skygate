#include "catalog/BundledCatalogParser.hpp"

#include "catalog/CatalogRowParser.hpp"

#include <string_view>

namespace skygate::ephemeris {
namespace {

constexpr std::string_view kBundledCatalogRows =
    "sun|Sun|Sun|-26.74\n"
    "moon|Moon|Moon|-12.74\n"
    "mercury|Mercury|Planet|-1.90\n"
    "venus|Venus|Planet|-4.92\n"
    "mars|Mars|Planet|-2.94\n"
    "jupiter|Jupiter|Planet|-2.94\n"
    "saturn|Saturn|Planet|-0.55\n"
    "uranus|Uranus|Planet|5.68\n"
    "neptune|Neptune|Planet|7.78\n"
    "sirius|Sirius|Star|-1.46\n"
    "canopus|Canopus|Star|-0.74\n"
    "arcturus|Arcturus|Star|-0.05\n"
    "vega|Vega|Star|0.03\n"
    "capella|Capella|Star|0.08\n"
    "rigel|Rigel|Star|0.12\n"
    "procyon|Procyon|Star|0.34\n"
    "betelgeuse|Betelgeuse|Star|0.50\n"
    "orion|Orion|Constellation|1.6\n"
    "ursa_major|Ursa Major|Constellation|1.8\n"
    "ursa_minor|Ursa Minor|Constellation|2.1\n"
    "cassiopeia|Cassiopeia|Constellation|2.2\n"
    "scorpius|Scorpius|Constellation|1.7\n"
    "cygnus|Cygnus|Constellation|1.3\n"
    "taurus|Taurus|Constellation|1.7\n"
    "leo|Leo|Constellation|1.4\n"
    "gemini|Gemini|Constellation|1.6\n"
    "andromeda|Andromeda|Constellation|2.1\n";

}  // namespace

CatalogBodyParseResult BundledCatalogParser::parse() const
{
    const CatalogRowParser parser;
    return parser.parse(kBundledCatalogRows);
}

}  // namespace skygate::ephemeris
