#include "CoreBodyCatalogAugmenter.hpp"

#include <array>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

struct BundledBrightStar final {
    std::string_view id;
    std::string_view displayName;
    double rightAscensionHours;
    double declinationDeg;
    double visualMagnitude;
};

constexpr std::array<BundledBrightStar, 8> kBundledBrightStars{{
    {"sirius", "Sirius", 6.7525, -16.7161, -1.46},
    {"canopus", "Canopus", 6.3992, -52.6957, -0.74},
    {"arcturus", "Arcturus", 14.2610, 19.1825, -0.05},
    {"vega", "Vega", 18.6156, 38.7837, 0.03},
    {"capella", "Capella", 5.2782, 45.9979, 0.08},
    {"rigel", "Rigel", 5.2423, -8.2016, 0.13},
    {"procyon", "Procyon", 7.6550, 5.2250, 0.34},
    {"betelgeuse", "Betelgeuse", 5.9195, 7.4071, 0.42},
}};

}  // namespace

std::vector<OwnGalaxyCelestialBody> CoreBodyCatalogAugmenter::bundledBrightStars()
{
    std::vector<OwnGalaxyCelestialBody> bodies;
    bodies.reserve(kBundledBrightStars.size());
    for (const BundledBrightStar& star : kBundledBrightStars) {
        OwnGalaxyCelestialBody body;
        body.id = star.id;
        body.displayName = star.displayName;
        body.kind = BaseCelestialBody::Kind::Star;
        body.visualMagnitude = star.visualMagnitude;
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = star.rightAscensionHours, .declinationDeg = star.declinationDeg
        };
        bodies.push_back(std::move(body));
    }
    return bodies;
}

}  // namespace skygate::ephemeris
