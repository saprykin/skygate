#pragma once

#include <QColor>

namespace skygate::ui::internal {

struct SkyViewportLineSegment final {
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;
    float widthPx = 1.0F;
    QColor color;
};

}  // namespace skygate::ui::internal
