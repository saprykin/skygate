#pragma once

#include "DeepSkyObjectInfo.hpp"

#include <QColor>

#include <algorithm>
#include <cstdint>

struct SkyRenderGlyph final {
    double x = 0.0;
    double y = 0.0;
    double radiusXPx = 4.0;
    double radiusYPx = 4.0;
    double rotationDeg = 0.0;
    double widthPx = 1.0;
    std::uint32_t bodyIndex = 0;
    skygate::ephemeris::DeepSkyObjectInfo::Kind kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Unknown;
    QColor color;

    // Largest axis radius, never below the caller's minimum. Label anchors and
    // hit targets need different minima, so the minimum stays a parameter.
    [[nodiscard]] double extentRadiusPx(double minimumRadiusPx) const noexcept
    {
        return std::max({radiusXPx, radiusYPx, minimumRadiusPx});
    }
};
