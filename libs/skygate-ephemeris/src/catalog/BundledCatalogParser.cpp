#include "catalog/BundledCatalogParser.hpp"

#include <array>
#include <string_view>

namespace skygate::ephemeris {
namespace {

struct BundledCatalogEntry final {
    std::string_view id;
    std::string_view displayName;
    CelestialBodyType type;
    double visualMagnitude;
};

constexpr std::array<BundledCatalogEntry, 27> kBundledCatalogEntries {{
    {"sun", "Sun", CelestialBodyType::Sun, -26.74},
    {"moon", "Moon", CelestialBodyType::Moon, -12.74},
    {"mercury", "Mercury", CelestialBodyType::Planet, -1.90},
    {"venus", "Venus", CelestialBodyType::Planet, -4.92},
    {"mars", "Mars", CelestialBodyType::Planet, -2.94},
    {"jupiter", "Jupiter", CelestialBodyType::Planet, -2.94},
    {"saturn", "Saturn", CelestialBodyType::Planet, -0.55},
    {"uranus", "Uranus", CelestialBodyType::Planet, 5.68},
    {"neptune", "Neptune", CelestialBodyType::Planet, 7.78},
    {"sirius", "Sirius", CelestialBodyType::Star, -1.46},
    {"canopus", "Canopus", CelestialBodyType::Star, -0.74},
    {"arcturus", "Arcturus", CelestialBodyType::Star, -0.05},
    {"vega", "Vega", CelestialBodyType::Star, 0.03},
    {"capella", "Capella", CelestialBodyType::Star, 0.08},
    {"rigel", "Rigel", CelestialBodyType::Star, 0.12},
    {"procyon", "Procyon", CelestialBodyType::Star, 0.34},
    {"betelgeuse", "Betelgeuse", CelestialBodyType::Star, 0.50},
    {"orion", "Orion", CelestialBodyType::Constellation, 1.6},
    {"ursa_major", "Ursa Major", CelestialBodyType::Constellation, 1.8},
    {"ursa_minor", "Ursa Minor", CelestialBodyType::Constellation, 2.1},
    {"cassiopeia", "Cassiopeia", CelestialBodyType::Constellation, 2.2},
    {"scorpius", "Scorpius", CelestialBodyType::Constellation, 1.7},
    {"cygnus", "Cygnus", CelestialBodyType::Constellation, 1.3},
    {"taurus", "Taurus", CelestialBodyType::Constellation, 1.7},
    {"leo", "Leo", CelestialBodyType::Constellation, 1.4},
    {"gemini", "Gemini", CelestialBodyType::Constellation, 1.6},
    {"andromeda", "Andromeda", CelestialBodyType::Constellation, 2.1},
}};

}  // namespace

CatalogBodyParseResult BundledCatalogParser::parse() const
{
    CatalogBodyParseResult result;
    result.bodies.reserve(kBundledCatalogEntries.size());
    for (const auto& entry : kBundledCatalogEntries) {
        result.bodies.push_back(CelestialBody {
            .id = std::string(entry.id),
            .displayName = std::string(entry.displayName),
            .type = entry.type,
            .visualMagnitude = entry.visualMagnitude,
        });
    }

    result.diagnostics.processedRowCount = kBundledCatalogEntries.size();
    result.diagnostics.parsedBodyCount = kBundledCatalogEntries.size();
    return result;
}

}  // namespace skygate::ephemeris
