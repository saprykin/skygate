#pragma once

struct SkyOverlayLayerVisibility final {
    bool horizon = true;
    bool altAzGrid = true;
    bool constellationLines = true;
    bool constellationLabels = true;
    bool ecliptic = false;
    bool celestialEquator = false;
    bool meridian = false;
    bool circumpolarBoundary = false;
    bool solarSystemLabels = true;

    [[nodiscard]] bool equals(const SkyOverlayLayerVisibility& other) const noexcept
    {
        return horizon == other.horizon
            && altAzGrid == other.altAzGrid
            && constellationLines == other.constellationLines
            && constellationLabels == other.constellationLabels
            && ecliptic == other.ecliptic
            && celestialEquator == other.celestialEquator
            && meridian == other.meridian
            && circumpolarBoundary == other.circumpolarBoundary
            && solarSystemLabels == other.solarSystemLabels;
    }
};
