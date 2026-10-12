#pragma once

#include <QColor>

struct SkyRenderLine final {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double widthPx = 1.0;
    QColor color;
};
