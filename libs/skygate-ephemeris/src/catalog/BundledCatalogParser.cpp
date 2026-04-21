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

constexpr std::array<BundledCatalogEntry, 9> kBundledCatalogEntries {{
    {"sun", "Sun", CelestialBodyType::Sun, -26.74},
    {"moon", "Moon", CelestialBodyType::Moon, -12.74},
    {"mercury", "Mercury", CelestialBodyType::Planet, -1.90},
    {"venus", "Venus", CelestialBodyType::Planet, -4.92},
    {"mars", "Mars", CelestialBodyType::Planet, -2.94},
    {"jupiter", "Jupiter", CelestialBodyType::Planet, -2.94},
    {"saturn", "Saturn", CelestialBodyType::Planet, -0.55},
    {"uranus", "Uranus", CelestialBodyType::Planet, 5.68},
    {"neptune", "Neptune", CelestialBodyType::Planet, 7.78},
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
