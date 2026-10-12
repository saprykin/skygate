#pragma once

#include "SkyRenderGlyph.hpp"
#include "SkyRenderLabel.hpp"
#include "SkyRenderLine.hpp"
#include "SkyRenderPoint.hpp"

#include <vector>

struct SkyRenderFrame final {
    std::vector<SkyRenderPoint> points;
    std::vector<SkyRenderLine> lines;
    std::vector<SkyRenderGlyph> glyphs;
    std::vector<SkyRenderLabel> labels;
};
