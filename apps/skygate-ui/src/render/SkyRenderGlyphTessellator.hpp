#pragma once

#include "SkyRenderGlyph.hpp"
#include "SkyViewportSegmentSink.hpp"

#include <span>

namespace skygate::ui::internal {

// Turns deep-sky glyph kinds into the line segments that draw them.
class SkyRenderGlyphTessellator final {
public:
    SkyRenderGlyphTessellator() = delete;

    static void appendGlyphs(SkyViewportSegmentSink& sink, std::span<const SkyRenderGlyph> renderGlyphs);
};

}  // namespace skygate::ui::internal
