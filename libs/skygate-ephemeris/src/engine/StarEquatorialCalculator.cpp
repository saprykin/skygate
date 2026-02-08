#include "engine/StarEquatorialCalculator.hpp"

#include <array>

namespace skygate::ephemeris {
namespace {

struct FixedStarCoordinate {
    const char* id;
    core::EquatorialCoordinate equatorial;
};

constexpr std::array<FixedStarCoordinate, 8> kFixedStarCoordinates = {{
    {.id = "sirius", .equatorial = {.rightAscensionHours = 6.7525, .declinationDeg = -16.7161}},
    {.id = "canopus", .equatorial = {.rightAscensionHours = 6.3992, .declinationDeg = -52.6957}},
    {.id = "arcturus", .equatorial = {.rightAscensionHours = 14.2610, .declinationDeg = 19.1825}},
    {.id = "vega", .equatorial = {.rightAscensionHours = 18.6156, .declinationDeg = 38.7837}},
    {.id = "capella", .equatorial = {.rightAscensionHours = 5.2782, .declinationDeg = 45.9979}},
    {.id = "rigel", .equatorial = {.rightAscensionHours = 5.2423, .declinationDeg = -8.2016}},
    {.id = "procyon", .equatorial = {.rightAscensionHours = 7.6550, .declinationDeg = 5.2250}},
    {.id = "betelgeuse", .equatorial = {.rightAscensionHours = 5.9195, .declinationDeg = 7.4071}},
}};

}  // namespace

std::optional<core::EquatorialCoordinate> StarEquatorialCalculator::compute(
    const std::string_view bodyId
) const noexcept
{
    for (const FixedStarCoordinate& star : kFixedStarCoordinates) {
        if (bodyId == star.id) {
            return star.equatorial;
        }
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
