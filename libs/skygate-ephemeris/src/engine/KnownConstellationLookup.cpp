#include "engine/KnownConstellationLookup.hpp"

#include <array>

namespace skygate::ephemeris {
namespace {

struct ConstellationCoordinate {
    const char* id;
    core::EquatorialCoordinate equatorial;
};

constexpr std::array<ConstellationCoordinate, 10> kKnownConstellationCoordinates = {{
    {.id = "orion", .equatorial = {.rightAscensionHours = 5.5833, .declinationDeg = 5.0}},
    {.id = "ursa_major", .equatorial = {.rightAscensionHours = 11.1667, .declinationDeg = 56.0}},
    {.id = "ursa_minor", .equatorial = {.rightAscensionHours = 15.0, .declinationDeg = 78.0}},
    {.id = "cassiopeia", .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 60.0}},
    {.id = "scorpius", .equatorial = {.rightAscensionHours = 16.8333, .declinationDeg = -30.0}},
    {.id = "cygnus", .equatorial = {.rightAscensionHours = 20.3333, .declinationDeg = 42.0}},
    {.id = "taurus", .equatorial = {.rightAscensionHours = 4.6667, .declinationDeg = 16.0}},
    {.id = "leo", .equatorial = {.rightAscensionHours = 10.6667, .declinationDeg = 15.0}},
    {.id = "gemini", .equatorial = {.rightAscensionHours = 7.0, .declinationDeg = 22.0}},
    {.id = "andromeda", .equatorial = {.rightAscensionHours = 1.1667, .declinationDeg = 38.0}},
}};

}  // namespace

std::optional<core::EquatorialCoordinate> KnownConstellationLookup::equatorialById(
    const std::string_view constellationId
) const noexcept
{
    for (const ConstellationCoordinate& constellation : kKnownConstellationCoordinates) {
        if (constellationId == constellation.id) {
            return constellation.equatorial;
        }
    }

    return std::nullopt;
}

}  // namespace skygate::ephemeris
