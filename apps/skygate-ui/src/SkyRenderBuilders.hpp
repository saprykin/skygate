#pragma once

#include <QColor>
#include <QVariantList>

#include "SkyTheme.hpp"
#include "skygate/core/PreparedProjection.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <span>
#include <vector>

struct SkyRenderPoint final {
    double x = 0.0;
    double y = 0.0;
    double sizePx = 2.0;
    std::uint32_t bodyIndex = 0;
    QColor color;
};

struct SkyRenderLine final {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double widthPx = 1.0;
    QColor color;
};

struct SkyRenderFrame final {
    std::vector<SkyRenderPoint> points;
    std::vector<SkyRenderLine> lines;
    QVariantList labels;
};

class SkyRenderFrameBuilder final {
public:
    [[nodiscard]] SkyRenderFrame buildFrame(
        const skygate::ephemeris::SkySnapshot& snapshot,
        const skygate::core::PreparedProjection& projection,
        std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
        std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
        double magnitudeCutoff,
        double viewportWidth,
        double viewportHeight,
        const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
    ) const;
};
