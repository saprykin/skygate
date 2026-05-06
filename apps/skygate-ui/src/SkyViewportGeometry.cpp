#include "SkyViewportGeometry.hpp"

#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/core/math/ProjectedPolylineBuilder.hpp"
#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace {

constexpr int kHorizonSampleCount = 96;
constexpr int kGridAltitudeStepDeg = 15;
constexpr int kGridAzimuthStepDeg = 30;
constexpr int kGridAltitudeSampleCount = 96;
constexpr int kGridAzimuthSampleCount = 64;
constexpr int kReferenceCircleSampleCount = 192;
constexpr float kGridLineWidthPx = 1.0F;
constexpr float kCardinalLineWidthPx = 1.8F;
constexpr float kHorizonLineWidthPx = 2.2F;
constexpr float kReferenceLineWidthPx = 1.4F;
constexpr int kGlyphEllipseSampleCount = 32;

[[nodiscard]] QColor cardinalAzimuthLineColor(
    const int azimuthDeg,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    switch (azimuthDeg) {
    case 0:
        return renderTheme.cardinalNorthLine;
    case 90:
        return renderTheme.cardinalEastLine;
    case 180:
        return renderTheme.cardinalSouthLine;
    case 270:
        return renderTheme.cardinalWestLine;
    default:
        return renderTheme.gridAzimuthLine;
    }
}

void appendLineSegment(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const float widthPx,
    const QColor& color
)
{
    lineSegments.push_back(skygate::ui::internal::SkyViewportLineSegment {
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .widthPx = widthPx,
        .color = color
    });
}

template <typename CoordinateFn>
void appendProjectedPolyline(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const skygate::core::PreparedProjection& projection,
    const int sampleCount,
    const double maxSegmentLengthSquared,
    const QColor& color,
    const float widthPx,
    CoordinateFn coordinateFn
)
{
    if (sampleCount <= 0) {
        return;
    }

    std::vector<skygate::core::HorizontalCoordinate> coordinates;
    coordinates.reserve(static_cast<std::size_t>(sampleCount) + 1U);
    for (int index = 0; index <= sampleCount; ++index) {
        coordinates.push_back(coordinateFn(index));
    }

    const skygate::core::ProjectedPolylineBuilder builder;
    for (const auto& segment : builder.build(projection, coordinates, maxSegmentLengthSquared)) {
        appendLineSegment(
            lineSegments,
            static_cast<float>(segment.x1),
            static_cast<float>(segment.y1),
            static_cast<float>(segment.x2),
            static_cast<float>(segment.y2),
            widthPx,
            color
        );
    }
}

[[nodiscard]] std::pair<float, float> rotatedGlyphPoint(
    const SkyRenderGlyph& glyph,
    const double x,
    const double y
)
{
    const auto point = skygate::core::rotatedOffsetPoint2d(
        glyph.x,
        glyph.y,
        x,
        y,
        glyph.rotationDeg
    );
    return {
        static_cast<float>(point.x),
        static_cast<float>(point.y)
    };
}

void appendGlyphCircle(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const SkyRenderGlyph& glyph,
    const double radiusXScale,
    const double radiusYScale
)
{
    const auto previousOffset = skygate::core::ellipseOffsetPoint2d(
        glyph.radiusXPx * radiusXScale,
        glyph.radiusYPx * radiusYScale,
        0,
        kGlyphEllipseSampleCount
    );
    auto previous = rotatedGlyphPoint(
        glyph,
        previousOffset.x,
        previousOffset.y
    );
    for (int index = 1; index <= kGlyphEllipseSampleCount; ++index) {
        const auto nextOffset = skygate::core::ellipseOffsetPoint2d(
            glyph.radiusXPx * radiusXScale,
            glyph.radiusYPx * radiusYScale,
            index,
            kGlyphEllipseSampleCount
        );
        const auto next = rotatedGlyphPoint(
            glyph,
            nextOffset.x,
            nextOffset.y
        );
        appendLineSegment(
            lineSegments,
            previous.first,
            previous.second,
            next.first,
            next.second,
            static_cast<float>(glyph.widthPx),
            glyph.color
        );
        previous = next;
    }
}

void appendGlyphCross(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const SkyRenderGlyph& glyph,
    const double scale
)
{
    const auto left = rotatedGlyphPoint(glyph, -glyph.radiusXPx * scale, 0.0);
    const auto right = rotatedGlyphPoint(glyph, glyph.radiusXPx * scale, 0.0);
    const auto top = rotatedGlyphPoint(glyph, 0.0, -glyph.radiusYPx * scale);
    const auto bottom = rotatedGlyphPoint(glyph, 0.0, glyph.radiusYPx * scale);
    appendLineSegment(
        lineSegments,
        left.first,
        left.second,
        right.first,
        right.second,
        static_cast<float>(glyph.widthPx),
        glyph.color
    );
    appendLineSegment(
        lineSegments,
        top.first,
        top.second,
        bottom.first,
        bottom.second,
        static_cast<float>(glyph.widthPx),
        glyph.color
    );
}

void appendGlyphDiamond(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const SkyRenderGlyph& glyph
)
{
    const std::array<std::pair<float, float>, 4> points {{
        rotatedGlyphPoint(glyph, 0.0, -glyph.radiusYPx),
        rotatedGlyphPoint(glyph, glyph.radiusXPx, 0.0),
        rotatedGlyphPoint(glyph, 0.0, glyph.radiusYPx),
        rotatedGlyphPoint(glyph, -glyph.radiusXPx, 0.0),
    }};
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& start = points[index];
        const auto& end = points[(index + 1U) % points.size()];
        appendLineSegment(
            lineSegments,
            start.first,
            start.second,
            end.first,
            end.second,
            static_cast<float>(glyph.widthPx),
            glyph.color
        );
    }
}

void appendDeepSkyGlyph(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const SkyRenderGlyph& glyph
)
{
    switch (glyph.kind) {
    case skygate::ephemeris::DeepSkyObjectKind::Galaxy:
        appendGlyphCircle(lineSegments, glyph, 1.0, 1.0);
        appendGlyphCircle(lineSegments, glyph, 0.58, 0.58);
        break;
    case skygate::ephemeris::DeepSkyObjectKind::OpenCluster:
    case skygate::ephemeris::DeepSkyObjectKind::Asterism:
        appendGlyphCircle(lineSegments, glyph, 1.0, 1.0);
        break;
    case skygate::ephemeris::DeepSkyObjectKind::GlobularCluster:
        appendGlyphCircle(lineSegments, glyph, 1.0, 1.0);
        appendGlyphCross(lineSegments, glyph, 0.62);
        break;
    case skygate::ephemeris::DeepSkyObjectKind::Nebula:
        appendGlyphDiamond(lineSegments, glyph);
        appendGlyphCircle(lineSegments, glyph, 0.68, 0.68);
        break;
    case skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula:
        appendGlyphCircle(lineSegments, glyph, 1.0, 1.0);
        appendGlyphCross(lineSegments, glyph, 1.0);
        break;
    case skygate::ephemeris::DeepSkyObjectKind::Unknown:
        appendGlyphDiamond(lineSegments, glyph);
        break;
    }
}

void appendAltAzGridLines(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    for (int altitudeDeg = -75; altitudeDeg <= 75; altitudeDeg += kGridAltitudeStepDeg) {
        if (altitudeDeg == 0) {
            continue;
        }

        appendProjectedPolyline(
            lineSegments,
            input.projection,
            kGridAltitudeSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.gridAltitudeLine,
            kGridLineWidthPx,
            [altitudeDeg](const int index) {
                const double azimuthDeg = (360.0 * static_cast<double>(index))
                                          / static_cast<double>(kGridAltitudeSampleCount);
                return skygate::core::HorizontalCoordinate {
                    .altitudeDeg = static_cast<double>(altitudeDeg),
                    .azimuthDeg = azimuthDeg
                };
            }
        );
    }

    for (int azimuthDeg = 0; azimuthDeg < 360; azimuthDeg += kGridAzimuthStepDeg) {
        if (azimuthDeg % 90 == 0) {
            continue;
        }

        appendProjectedPolyline(
            lineSegments,
            input.projection,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.gridAzimuthLine,
            kGridLineWidthPx,
            [azimuthDeg](const int index) {
                const double altitudeDeg = -85.0 + (170.0 * static_cast<double>(index))
                                                       / static_cast<double>(kGridAzimuthSampleCount);
                return skygate::core::HorizontalCoordinate {
                    .altitudeDeg = altitudeDeg,
                    .azimuthDeg = static_cast<double>(azimuthDeg)
                };
            }
        );
    }
}

void appendCardinalGridLines(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    constexpr std::array<int, 4> kCardinalAzimuths {0, 90, 180, 270};
    for (const int cardinalAzimuthDeg : kCardinalAzimuths) {
        appendProjectedPolyline(
            lineSegments,
            input.projection,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            cardinalAzimuthLineColor(cardinalAzimuthDeg, input.renderTheme),
            kCardinalLineWidthPx,
            [cardinalAzimuthDeg](const int index) {
                const double altitudeDeg = -85.0 + (170.0 * static_cast<double>(index))
                                                       / static_cast<double>(kGridAzimuthSampleCount);
                return skygate::core::HorizontalCoordinate {
                    .altitudeDeg = altitudeDeg,
                    .azimuthDeg = static_cast<double>(cardinalAzimuthDeg)
                };
            }
        );
    }
}

void appendHorizonLine(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    appendProjectedPolyline(
        lineSegments,
        input.projection,
        kHorizonSampleCount,
        maxSegmentLengthSquared,
        input.renderTheme.horizonLine,
        kHorizonLineWidthPx,
        [](const int index) {
            const double azimuthDeg = (360.0 * static_cast<double>(index))
                                      / static_cast<double>(kHorizonSampleCount);
            return skygate::core::HorizontalCoordinate {.altitudeDeg = 0.0, .azimuthDeg = azimuthDeg};
        }
    );
}

void appendReferenceLines(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    if (input.overlayLayers.ecliptic) {
        appendProjectedPolyline(
            lineSegments,
            input.projection,
            kReferenceCircleSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.eclipticLine,
            kReferenceLineWidthPx,
            [&input](const int index) {
                const double eclipticLongitudeDeg = 360.0 * static_cast<double>(index)
                    / static_cast<double>(kReferenceCircleSampleCount);
                return skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(
                    eclipticLongitudeDeg,
                    input.observer,
                    input.utcTime
                );
            }
        );
    }

    if (input.overlayLayers.celestialEquator) {
        appendProjectedPolyline(
            lineSegments,
            input.projection,
            kReferenceCircleSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.celestialEquatorLine,
            kReferenceLineWidthPx,
            [&input](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index,
                    kReferenceCircleSampleCount,
                    0.0,
                    input.observer,
                    input.utcTime
                );
            }
        );
    }

    if (
        input.overlayLayers.circumpolarBoundary
        && input.observer.isValid()
    ) {
        const double boundaryDeclinationDeg =
            skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
                input.observer
            );
        appendProjectedPolyline(
            lineSegments,
            input.projection,
            kReferenceCircleSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.circumpolarBoundaryLine,
            kReferenceLineWidthPx,
            [boundaryDeclinationDeg, &input](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index,
                    kReferenceCircleSampleCount,
                    boundaryDeclinationDeg,
                    input.observer,
                    input.utcTime
                );
            }
        );
    }
}

void appendFrameLines(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const std::span<const SkyRenderLine> renderLines
)
{
    for (const auto& line : renderLines) {
        appendLineSegment(
            lineSegments,
            static_cast<float>(line.x1),
            static_cast<float>(line.y1),
            static_cast<float>(line.x2),
            static_cast<float>(line.y2),
            static_cast<float>(line.widthPx),
            line.color
        );
    }
}

void appendFrameGlyphs(
    std::vector<skygate::ui::internal::SkyViewportLineSegment>& lineSegments,
    const std::span<const SkyRenderGlyph> renderGlyphs
)
{
    for (const auto& glyph : renderGlyphs) {
        appendDeepSkyGlyph(lineSegments, glyph);
    }
}

}  // namespace

namespace skygate::ui::internal {

std::vector<SkyViewportLineSegment> buildSkyViewportLineSegments(
    const SkyViewportGeometryInput& input
)
{
    const double maxSegmentLength = std::max(input.viewportWidth, input.viewportHeight) * 0.30;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;

    std::vector<SkyViewportLineSegment> lineSegments;
    lineSegments.reserve(4096U);

    if (input.overlayLayers.altAzGrid) {
        appendAltAzGridLines(lineSegments, input, maxSegmentLengthSquared);
        appendCardinalGridLines(lineSegments, input, maxSegmentLengthSquared);
    }

    if (input.overlayLayers.horizon) {
        appendHorizonLine(lineSegments, input, maxSegmentLengthSquared);
    }

    appendReferenceLines(lineSegments, input, maxSegmentLengthSquared);
    appendFrameLines(lineSegments, input.renderLines);
    appendFrameGlyphs(lineSegments, input.renderGlyphs);

    return lineSegments;
}

}  // namespace skygate::ui::internal
