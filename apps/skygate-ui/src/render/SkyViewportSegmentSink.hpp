#pragma once

#include "SkyViewportLineSegment.hpp"

#include <QColor>

#include <vector>

namespace skygate::ui::internal {

// Target for the line segments the overlay and glyph builders produce. The
// segments keep their append order because the scene graph buckets by color
// while walking them.
class SkyViewportSegmentSink final {
public:
    explicit SkyViewportSegmentSink(std::vector<SkyViewportLineSegment>& segments) noexcept : m_segments(segments) {}

    void
    append(const float x1, const float y1, const float x2, const float y2, const float widthPx, const QColor& color)
    {
        m_segments.push_back(
            SkyViewportLineSegment{.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2, .widthPx = widthPx, .color = color}
        );
    }

private:
    std::vector<SkyViewportLineSegment>& m_segments;
};

}  // namespace skygate::ui::internal
