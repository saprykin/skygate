#pragma once

#include <QColor>

#include <cstdint>

struct SkyRenderPoint final {
    double x = 0.0;
    double y = 0.0;
    double sizePx = 2.0;
    std::uint32_t bodyIndex = 0;
    QColor color;
};
