#pragma once

#include <QColor>
#include <QString>

#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
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

struct SkyRenderGlyph final {
    double x = 0.0;
    double y = 0.0;
    double radiusXPx = 4.0;
    double radiusYPx = 4.0;
    double rotationDeg = 0.0;
    double widthPx = 1.0;
    std::uint32_t bodyIndex = 0;
    skygate::ephemeris::DeepSkyObjectKind kind =
        skygate::ephemeris::DeepSkyObjectKind::Unknown;
    QColor color;
};

struct SkyRenderLabel final {
    QString kind;
    double x = 0.0;
    double y = 0.0;
    QString text;
    QColor color;
};

struct SkyRenderFrame final {
    std::vector<SkyRenderPoint> points;
    std::vector<SkyRenderLine> lines;
    std::vector<SkyRenderGlyph> glyphs;
    std::vector<SkyRenderLabel> labels;
};
