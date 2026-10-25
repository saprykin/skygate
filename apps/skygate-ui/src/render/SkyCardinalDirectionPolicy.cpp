#include "SkyCardinalDirectionPolicy.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <array>

namespace skygate::ui::internal {
namespace {

constexpr std::array<SkyCardinalDirectionPolicy::Direction, 4> kDirections{{
    {.azimuthDeg = 0.0, .label = "N", .lineColor = &SkyThemeRenderPalette::cardinalNorthLine},
    {.azimuthDeg = 90.0, .label = "E", .lineColor = &SkyThemeRenderPalette::cardinalEastLine},
    {.azimuthDeg = 180.0, .label = "S", .lineColor = &SkyThemeRenderPalette::cardinalSouthLine},
    {.azimuthDeg = 270.0, .label = "W", .lineColor = &SkyThemeRenderPalette::cardinalWestLine},
}};

}  // namespace

std::span<const SkyCardinalDirectionPolicy::Direction> SkyCardinalDirectionPolicy::directions() noexcept
{
    return kDirections;
}

}  // namespace skygate::ui::internal
