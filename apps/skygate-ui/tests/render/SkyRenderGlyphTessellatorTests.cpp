#include "DeepSkyObjectInfo.hpp"
#include "math/MathConstants.hpp"
#include "render/SkyRenderGlyph.hpp"
#include "render/SkyRenderGlyphTessellator.hpp"
#include "render/SkyViewportLineSegment.hpp"
#include "render/SkyViewportSegmentSink.hpp"

#include <QColor>
#include <QtTest/QtTest>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

namespace {

using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ui::internal::SkyRenderGlyphTessellator;
using skygate::ui::internal::SkyViewportLineSegment;
using skygate::ui::internal::SkyViewportSegmentSink;

// Every ellipse ring is sampled at this many vertices, so it emits one segment
// per vertex.
constexpr std::size_t kCircleSegmentCount = 32U;
constexpr std::size_t kDiamondSegmentCount = 4U;
constexpr std::size_t kCrossSegmentCount = 2U;

constexpr double kCoordinateTolerance = 2.0e-3;
constexpr double kAreaTolerance = 0.1;
constexpr double kEllipseTolerance = 1.0e-4;

SkyRenderGlyph makeGlyph(const DeepSkyObjectInfo::Kind kind)
{
    SkyRenderGlyph glyph;
    glyph.x = 480.0;
    glyph.y = 320.0;
    glyph.radiusXPx = 18.0;
    glyph.radiusYPx = 6.0;
    glyph.widthPx = 1.5;
    glyph.color = QColor("#00ffff");
    glyph.kind = kind;
    return glyph;
}

std::vector<SkyViewportLineSegment> tessellate(const std::span<const SkyRenderGlyph> glyphs)
{
    std::vector<SkyViewportLineSegment> segments;
    SkyViewportSegmentSink sink(segments);
    SkyRenderGlyphTessellator::appendGlyphs(sink, glyphs);
    return segments;
}

std::vector<SkyViewportLineSegment> tessellate(const SkyRenderGlyph& glyph)
{
    return tessellate(std::span<const SkyRenderGlyph>(&glyph, 1U));
}

bool nearlyEqual(const double actual, const double expected, const double tolerance = kCoordinateTolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

bool startsAt(const SkyViewportLineSegment& segment, const double x, const double y)
{
    return nearlyEqual(segment.x1, x) && nearlyEqual(segment.y1, y);
}

bool endsAt(const SkyViewportLineSegment& segment, const double x, const double y)
{
    return nearlyEqual(segment.x2, x) && nearlyEqual(segment.y2, y);
}

// True when every segment starts where its predecessor ended and the last
// segment returns to the first start point.
bool closesLoop(const std::span<const SkyViewportLineSegment> segments)
{
    if (segments.empty()) {
        return false;
    }
    for (std::size_t index = 1; index < segments.size(); ++index) {
        if (!nearlyEqual(segments[index - 1U].x2, segments[index].x1)
            || !nearlyEqual(segments[index - 1U].y2, segments[index].y1)) {
            return false;
        }
    }
    return nearlyEqual(segments.back().x2, segments.front().x1) && nearlyEqual(segments.back().y2, segments.front().y1);
}

// Shoelace sum of a closed polyline: the sign reports the winding direction and
// the magnitude the enclosed area.
double signedArea(const std::span<const SkyViewportLineSegment> segments)
{
    double twiceArea = 0.0;
    for (const SkyViewportLineSegment& segment : segments) {
        twiceArea += (static_cast<double>(segment.x1) * static_cast<double>(segment.y2))
                     - (static_cast<double>(segment.x2) * static_cast<double>(segment.y1));
    }
    return twiceArea * 0.5;
}

double expectedCircleArea(const double radiusX, const double radiusY)
{
    using skygate::core::MathConstants;
    const double angleStep = MathConstants::kTwoPi / static_cast<double>(kCircleSegmentCount);
    return 0.5 * static_cast<double>(kCircleSegmentCount) * radiusX * radiusY * std::sin(angleStep);
}

bool offsetOnEllipse(
    const double x,
    const double y,
    const double centerX,
    const double centerY,
    const double radiusX,
    const double radiusY
)
{
    const double offsetX = (x - centerX) / radiusX;
    const double offsetY = (y - centerY) / radiusY;
    return std::abs(((offsetX * offsetX) + (offsetY * offsetY)) - 1.0) <= kEllipseTolerance;
}

bool segmentStartsOnEllipse(
    const SkyViewportLineSegment& segment,
    const double centerX,
    const double centerY,
    const double radiusX,
    const double radiusY
)
{
    return offsetOnEllipse(segment.x1, segment.y1, centerX, centerY, radiusX, radiusY);
}

struct Bounds final {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

Bounds segmentBounds(const std::span<const SkyViewportLineSegment> segments)
{
    Bounds bounds{
        .minX = static_cast<double>(segments.front().x1),
        .minY = static_cast<double>(segments.front().y1),
        .maxX = static_cast<double>(segments.front().x1),
        .maxY = static_cast<double>(segments.front().y1),
    };
    for (const SkyViewportLineSegment& segment : segments) {
        for (const double x : {static_cast<double>(segment.x1), static_cast<double>(segment.x2)}) {
            bounds.minX = std::min(bounds.minX, x);
            bounds.maxX = std::max(bounds.maxX, x);
        }
        for (const double y : {static_cast<double>(segment.y1), static_cast<double>(segment.y2)}) {
            bounds.minY = std::min(bounds.minY, y);
            bounds.maxY = std::max(bounds.maxY, y);
        }
    }
    return bounds;
}

}  // namespace

class SkyRenderGlyphTessellatorTests final : public QObject {
    Q_OBJECT

private slots:
    void glyphKindsEmitExpectedSegmentCounts();
    void emptyGlyphSpanAppendsNoSegments();
    void circleGlyphFormsClosedCounterClockwiseLoop();
    void circleGlyphVerticesRespectRadiiAndBounds();
    void glyphRotationRotatesOffsetsAboutCenter();
    void crossGlyphSegmentsFollowScaledRadii();
    void globularClusterCrossUsesInnerRadii();
    void galaxyGlyphDrawsTwoConcentricCircles();
    void nebulaGlyphDrawsDiamondThenInnerCircle();
    void unknownGlyphDrawsRotatedDiamond();
    void glyphGroupsKeepAppendOrderAndStyle();
};

void SkyRenderGlyphTessellatorTests::glyphKindsEmitExpectedSegmentCounts()
{
    QCOMPARE(tessellate(makeGlyph(DeepSkyObjectInfo::Kind::Unknown)).size(), kDiamondSegmentCount);
    QCOMPARE(tessellate(makeGlyph(DeepSkyObjectInfo::Kind::Galaxy)).size(), 2U * kCircleSegmentCount);
    QCOMPARE(tessellate(makeGlyph(DeepSkyObjectInfo::Kind::OpenCluster)).size(), kCircleSegmentCount);
    QCOMPARE(tessellate(makeGlyph(DeepSkyObjectInfo::Kind::Asterism)).size(), kCircleSegmentCount);
    QCOMPARE(
        tessellate(makeGlyph(DeepSkyObjectInfo::Kind::GlobularCluster)).size(), kCircleSegmentCount + kCrossSegmentCount
    );
    QCOMPARE(tessellate(makeGlyph(DeepSkyObjectInfo::Kind::Nebula)).size(), kDiamondSegmentCount + kCircleSegmentCount);
    QCOMPARE(
        tessellate(makeGlyph(DeepSkyObjectInfo::Kind::PlanetaryNebula)).size(), kCircleSegmentCount + kCrossSegmentCount
    );
}

void SkyRenderGlyphTessellatorTests::emptyGlyphSpanAppendsNoSegments()
{
    const std::vector<SkyRenderGlyph> glyphs;
    QVERIFY(tessellate(glyphs).empty());
}

void SkyRenderGlyphTessellatorTests::circleGlyphFormsClosedCounterClockwiseLoop()
{
    const SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::OpenCluster);
    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kCircleSegmentCount);

    QVERIFY(closesLoop(segments));

    const double area = signedArea(segments);
    QVERIFY(area > 0.0);
    QVERIFY(nearlyEqual(area, expectedCircleArea(glyph.radiusXPx, glyph.radiusYPx), kAreaTolerance));
}

void SkyRenderGlyphTessellatorTests::circleGlyphVerticesRespectRadiiAndBounds()
{
    const SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::OpenCluster);
    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kCircleSegmentCount);

    for (const SkyViewportLineSegment& segment : segments) {
        QVERIFY(segmentStartsOnEllipse(segment, glyph.x, glyph.y, glyph.radiusXPx, glyph.radiusYPx));
        QVERIFY(offsetOnEllipse(segment.x2, segment.y2, glyph.x, glyph.y, glyph.radiusXPx, glyph.radiusYPx));
        QCOMPARE(segment.widthPx, static_cast<float>(glyph.widthPx));
        QCOMPARE(segment.color, glyph.color);
    }

    const Bounds bounds = segmentBounds(segments);
    QVERIFY(nearlyEqual(bounds.minX, glyph.x - glyph.radiusXPx));
    QVERIFY(nearlyEqual(bounds.maxX, glyph.x + glyph.radiusXPx));
    QVERIFY(nearlyEqual(bounds.minY, glyph.y - glyph.radiusYPx));
    QVERIFY(nearlyEqual(bounds.maxY, glyph.y + glyph.radiusYPx));
}

void SkyRenderGlyphTessellatorTests::glyphRotationRotatesOffsetsAboutCenter()
{
    SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::OpenCluster);
    glyph.rotationDeg = 90.0;

    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kCircleSegmentCount);

    // Sampling starts at the positive x offset, which a quarter turn maps onto
    // the positive y axis.
    QVERIFY(startsAt(segments.front(), glyph.x, glyph.y + glyph.radiusXPx));

    const Bounds bounds = segmentBounds(segments);
    QVERIFY(nearlyEqual(bounds.minX, glyph.x - glyph.radiusYPx));
    QVERIFY(nearlyEqual(bounds.maxX, glyph.x + glyph.radiusYPx));
    QVERIFY(nearlyEqual(bounds.minY, glyph.y - glyph.radiusXPx));
    QVERIFY(nearlyEqual(bounds.maxY, glyph.y + glyph.radiusXPx));
    QVERIFY(closesLoop(segments));
    QVERIFY(signedArea(segments) > 0.0);
}

void SkyRenderGlyphTessellatorTests::crossGlyphSegmentsFollowScaledRadii()
{
    const SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::PlanetaryNebula);
    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kCircleSegmentCount + kCrossSegmentCount);

    const SkyViewportLineSegment& horizontal = segments[kCircleSegmentCount];
    QVERIFY(startsAt(horizontal, glyph.x - glyph.radiusXPx, glyph.y));
    QVERIFY(endsAt(horizontal, glyph.x + glyph.radiusXPx, glyph.y));

    const SkyViewportLineSegment& vertical = segments[kCircleSegmentCount + 1U];
    QVERIFY(startsAt(vertical, glyph.x, glyph.y - glyph.radiusYPx));
    QVERIFY(endsAt(vertical, glyph.x, glyph.y + glyph.radiusYPx));

    QCOMPARE(horizontal.widthPx, static_cast<float>(glyph.widthPx));
    QCOMPARE(vertical.color, glyph.color);
}

void SkyRenderGlyphTessellatorTests::globularClusterCrossUsesInnerRadii()
{
    const SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::GlobularCluster);
    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kCircleSegmentCount + kCrossSegmentCount);

    constexpr double kCrossScale = 0.62;
    const SkyViewportLineSegment& horizontal = segments[kCircleSegmentCount];
    QVERIFY(startsAt(horizontal, glyph.x - (glyph.radiusXPx * kCrossScale), glyph.y));
    QVERIFY(endsAt(horizontal, glyph.x + (glyph.radiusXPx * kCrossScale), glyph.y));

    const SkyViewportLineSegment& vertical = segments[kCircleSegmentCount + 1U];
    QVERIFY(startsAt(vertical, glyph.x, glyph.y - (glyph.radiusYPx * kCrossScale)));
    QVERIFY(endsAt(vertical, glyph.x, glyph.y + (glyph.radiusYPx * kCrossScale)));
}

void SkyRenderGlyphTessellatorTests::galaxyGlyphDrawsTwoConcentricCircles()
{
    const SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::Galaxy);
    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), 2U * kCircleSegmentCount);

    const std::span<const SkyViewportLineSegment> outer(segments.data(), kCircleSegmentCount);
    const std::span<const SkyViewportLineSegment> inner(segments.data() + kCircleSegmentCount, kCircleSegmentCount);

    QVERIFY(closesLoop(outer));
    QVERIFY(closesLoop(inner));

    constexpr double kInnerScale = 0.58;
    for (const SkyViewportLineSegment& segment : outer) {
        QVERIFY(segmentStartsOnEllipse(segment, glyph.x, glyph.y, glyph.radiusXPx, glyph.radiusYPx));
    }
    for (const SkyViewportLineSegment& segment : inner) {
        QVERIFY(segmentStartsOnEllipse(
            segment, glyph.x, glyph.y, glyph.radiusXPx * kInnerScale, glyph.radiusYPx * kInnerScale
        ));
    }

    QVERIFY(nearlyEqual(
        signedArea(inner),
        expectedCircleArea(glyph.radiusXPx * kInnerScale, glyph.radiusYPx * kInnerScale),
        kAreaTolerance
    ));
    QVERIFY(signedArea(outer) > signedArea(inner));
}

void SkyRenderGlyphTessellatorTests::nebulaGlyphDrawsDiamondThenInnerCircle()
{
    const SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::Nebula);
    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kDiamondSegmentCount + kCircleSegmentCount);

    const std::span<const SkyViewportLineSegment> diamond(segments.data(), kDiamondSegmentCount);
    QVERIFY(closesLoop(diamond));
    QVERIFY(startsAt(diamond[0], glyph.x, glyph.y - glyph.radiusYPx));
    QVERIFY(endsAt(diamond[0], glyph.x + glyph.radiusXPx, glyph.y));
    QVERIFY(startsAt(diamond[1], glyph.x + glyph.radiusXPx, glyph.y));
    QVERIFY(endsAt(diamond[1], glyph.x, glyph.y + glyph.radiusYPx));
    QVERIFY(startsAt(diamond[2], glyph.x, glyph.y + glyph.radiusYPx));
    QVERIFY(endsAt(diamond[2], glyph.x - glyph.radiusXPx, glyph.y));
    QVERIFY(startsAt(diamond[3], glyph.x - glyph.radiusXPx, glyph.y));
    QVERIFY(endsAt(diamond[3], glyph.x, glyph.y - glyph.radiusYPx));
    QVERIFY(signedArea(diamond) > 0.0);

    constexpr double kInnerScale = 0.68;
    const std::span<const SkyViewportLineSegment> inner(segments.data() + kDiamondSegmentCount, kCircleSegmentCount);
    QVERIFY(closesLoop(inner));
    for (const SkyViewportLineSegment& segment : inner) {
        QVERIFY(segmentStartsOnEllipse(
            segment, glyph.x, glyph.y, glyph.radiusXPx * kInnerScale, glyph.radiusYPx * kInnerScale
        ));
    }
}

void SkyRenderGlyphTessellatorTests::unknownGlyphDrawsRotatedDiamond()
{
    SkyRenderGlyph glyph = makeGlyph(DeepSkyObjectInfo::Kind::Unknown);
    glyph.rotationDeg = 45.0;

    const auto segments = tessellate(glyph);
    QCOMPARE(segments.size(), kDiamondSegmentCount);
    QVERIFY(closesLoop(segments));
    QVERIFY(signedArea(segments) > 0.0);

    // A quarter turn of the diamond puts its horizontal radius on the diagonal,
    // which is wider than the vertical radius and therefore sets the bounds.
    constexpr double kDiagonalScale = 0.70710678118654752;
    QVERIFY(startsAt(
        segments.front(), glyph.x + (kDiagonalScale * glyph.radiusYPx), glyph.y - (kDiagonalScale * glyph.radiusYPx)
    ));

    const Bounds bounds = segmentBounds(segments);
    QVERIFY(nearlyEqual(bounds.minX, glyph.x - (kDiagonalScale * glyph.radiusXPx)));
    QVERIFY(nearlyEqual(bounds.maxX, glyph.x + (kDiagonalScale * glyph.radiusXPx)));
    QVERIFY(nearlyEqual(bounds.minY, glyph.y - (kDiagonalScale * glyph.radiusXPx)));
    QVERIFY(nearlyEqual(bounds.maxY, glyph.y + (kDiagonalScale * glyph.radiusXPx)));
}

void SkyRenderGlyphTessellatorTests::glyphGroupsKeepAppendOrderAndStyle()
{
    SkyRenderGlyph circleGlyph = makeGlyph(DeepSkyObjectInfo::Kind::OpenCluster);
    circleGlyph.color = QColor("#00ffff");
    circleGlyph.widthPx = 1.5;
    SkyRenderGlyph diamondGlyph = makeGlyph(DeepSkyObjectInfo::Kind::Unknown);
    diamondGlyph.x = 120.0;
    diamondGlyph.y = 90.0;
    diamondGlyph.color = QColor("#ff8800");
    diamondGlyph.widthPx = 3.0;

    const std::vector<SkyRenderGlyph> glyphs{circleGlyph, diamondGlyph};
    const auto segments = tessellate(glyphs);
    QCOMPARE(segments.size(), kCircleSegmentCount + kDiamondSegmentCount);

    for (std::size_t index = 0; index < kCircleSegmentCount; ++index) {
        QCOMPARE(segments[index].color, circleGlyph.color);
        QCOMPARE(segments[index].widthPx, static_cast<float>(circleGlyph.widthPx));
        QVERIFY(segmentStartsOnEllipse(
            segments[index], circleGlyph.x, circleGlyph.y, circleGlyph.radiusXPx, circleGlyph.radiusYPx
        ));
    }

    const std::span<const SkyViewportLineSegment> diamond(segments.data() + kCircleSegmentCount, kDiamondSegmentCount);
    for (const SkyViewportLineSegment& segment : diamond) {
        QCOMPARE(segment.color, diamondGlyph.color);
        QCOMPARE(segment.widthPx, static_cast<float>(diamondGlyph.widthPx));
    }
    const Bounds bounds = segmentBounds(diamond);
    QVERIFY(nearlyEqual(bounds.minX, diamondGlyph.x - diamondGlyph.radiusXPx));
    QVERIFY(nearlyEqual(bounds.maxX, diamondGlyph.x + diamondGlyph.radiusXPx));
    QVERIFY(nearlyEqual(bounds.minY, diamondGlyph.y - diamondGlyph.radiusYPx));
    QVERIFY(nearlyEqual(bounds.maxY, diamondGlyph.y + diamondGlyph.radiusYPx));
}

QTEST_APPLESS_MAIN(SkyRenderGlyphTessellatorTests)

#include "SkyRenderGlyphTessellatorTests.moc"
