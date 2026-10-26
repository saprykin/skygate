#include "SkyViewportGeometry.hpp"
#include "SkyCardinalDirectionPolicy.hpp"
#include "SkyRenderGlyphTessellator.hpp"
#include "SkyReferenceOverlayLayers.hpp"
#include "SkyRenderLine.hpp"
#include "SkyViewportGeometryInput.hpp"
#include "SkyViewportLineSegment.hpp"
#include "SkyViewportSegmentSink.hpp"
#include "math/ProjectedPolylineBuilder.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

namespace {

constexpr int kHorizonSampleCount = 96;
constexpr int kGridAltitudeStepDeg = 15;
constexpr int kGridAzimuthStepDeg = 30;
constexpr int kGridAltitudeSampleCount = 96;
constexpr int kGridAzimuthSampleCount = 64;
// Azimuth lines stop short of the poles, where every azimuth projects to the
// same point.
constexpr double kGridMeridianAltitudeMinDeg = -85.0;
constexpr double kGridMeridianAltitudeMaxDeg = 85.0;
// Projected chords longer than this share of the larger viewport dimension are
// dropped, which removes the screen-wide jumps where geometry wraps.
constexpr double kOverlaySegmentLengthViewportFraction = 0.30;
constexpr float kGridLineWidthPx = 1.0F;
constexpr float kCardinalLineWidthPx = 1.8F;
constexpr float kHorizonLineWidthPx = 2.2F;
constexpr float kReferenceLineWidthPx = 1.4F;

[[nodiscard]] skygate::core::HorizontalCoordinate
meridianPoint(const double azimuthDeg, const int sampleIndex, const int sampleCount)
{
    const double altitudeDeg =
        kGridMeridianAltitudeMinDeg
        + ((kGridMeridianAltitudeMaxDeg - kGridMeridianAltitudeMinDeg) * static_cast<double>(sampleIndex))
              / static_cast<double>(sampleCount);
    return skygate::core::HorizontalCoordinate{.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg};
}

template <typename CoordinateFn>
void appendProjectedPolyline(
    skygate::ui::internal::SkyViewportSegmentSink& sink,
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
        sink.append(
            static_cast<float>(segment.x1),
            static_cast<float>(segment.y1),
            static_cast<float>(segment.x2),
            static_cast<float>(segment.y2),
            widthPx,
            color
        );
    }
}

void appendAltAzGridLines(
    skygate::ui::internal::SkyViewportSegmentSink& sink,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    for (int altitudeDeg = -75; altitudeDeg <= 75; altitudeDeg += kGridAltitudeStepDeg) {
        if (altitudeDeg == 0) {
            continue;
        }

        appendProjectedPolyline(
            sink,
            input.projection,
            kGridAltitudeSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.gridAltitudeLine,
            kGridLineWidthPx,
            [altitudeDeg](const int index) {
                const double azimuthDeg =
                    (360.0 * static_cast<double>(index)) / static_cast<double>(kGridAltitudeSampleCount);
                return skygate::core::HorizontalCoordinate{
                    .altitudeDeg = static_cast<double>(altitudeDeg), .azimuthDeg = azimuthDeg
                };
            }
        );
    }

    for (int azimuthDeg = 0; azimuthDeg < 360; azimuthDeg += kGridAzimuthStepDeg) {
        if (azimuthDeg % 90 == 0) {
            continue;
        }

        appendProjectedPolyline(
            sink,
            input.projection,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            input.renderTheme.gridAzimuthLine,
            kGridLineWidthPx,
            [azimuthDeg](const int index) {
                return meridianPoint(static_cast<double>(azimuthDeg), index, kGridAzimuthSampleCount);
            }
        );
    }
}

void appendCardinalGridLines(
    skygate::ui::internal::SkyViewportSegmentSink& sink,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    for (const auto& direction : skygate::ui::internal::SkyCardinalDirectionPolicy::directions()) {
        appendProjectedPolyline(
            sink,
            input.projection,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            direction.color(input.renderTheme),
            kCardinalLineWidthPx,
            [azimuthDeg = direction.azimuthDeg](const int index) {
                return meridianPoint(azimuthDeg, index, kGridAzimuthSampleCount);
            }
        );
    }
}

void appendHorizonLine(
    skygate::ui::internal::SkyViewportSegmentSink& sink,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    appendProjectedPolyline(
        sink,
        input.projection,
        kHorizonSampleCount,
        maxSegmentLengthSquared,
        input.renderTheme.horizonLine,
        kHorizonLineWidthPx,
        [](const int index) {
            const double azimuthDeg = (360.0 * static_cast<double>(index)) / static_cast<double>(kHorizonSampleCount);
            return skygate::core::HorizontalCoordinate{.altitudeDeg = 0.0, .azimuthDeg = azimuthDeg};
        }
    );
}

void appendReferenceLines(
    skygate::ui::internal::SkyViewportSegmentSink& sink,
    const skygate::ui::internal::SkyViewportGeometryInput& input,
    const double maxSegmentLengthSquared
)
{
    for (const auto& layer : skygate::ui::internal::SkyReferenceOverlayLayers::layers(
             input.renderTheme, input.overlayLayers, input.observer
         )) {
        if (!layer.enabled) {
            continue;
        }

        appendProjectedPolyline(
            sink,
            input.projection,
            skygate::ui::internal::SkyReferenceOverlayLayers::kSampleCount,
            maxSegmentLengthSquared,
            layer.color,
            kReferenceLineWidthPx,
            [&layer, &input](const int index) {
                return skygate::ui::internal::SkyReferenceOverlayLayers::samplePoint(
                    layer.circle, index, input.observer, input.utcTime
                );
            }
        );
    }
}

void appendFrameLines(
    skygate::ui::internal::SkyViewportSegmentSink& sink, const std::span<const SkyRenderLine> renderLines
)
{
    for (const auto& line : renderLines) {
        sink.append(
            static_cast<float>(line.x1),
            static_cast<float>(line.y1),
            static_cast<float>(line.x2),
            static_cast<float>(line.y2),
            static_cast<float>(line.widthPx),
            line.color
        );
    }
}

}  // namespace

namespace skygate::ui::internal {

std::vector<SkyViewportLineSegment> SkyViewportGeometry::buildLineSegments(const SkyViewportGeometryInput& input)
{
    const double maxSegmentLengthSquared = skygate::core::ProjectedPolylineBuilder::viewportSegmentLimitSquared(
        input.viewportWidth, input.viewportHeight, kOverlaySegmentLengthViewportFraction
    );

    std::vector<SkyViewportLineSegment> lineSegments;
    lineSegments.reserve(4096U);
    SkyViewportSegmentSink sink(lineSegments);

    if (input.overlayLayers.altAzGrid) {
        appendAltAzGridLines(sink, input, maxSegmentLengthSquared);
        appendCardinalGridLines(sink, input, maxSegmentLengthSquared);
    }

    if (input.overlayLayers.horizon) {
        appendHorizonLine(sink, input, maxSegmentLengthSquared);
    }

    appendReferenceLines(sink, input, maxSegmentLengthSquared);
    appendFrameLines(sink, input.renderLines);
    SkyRenderGlyphTessellator::appendGlyphs(sink, input.renderGlyphs);

    return lineSegments;
}

}  // namespace skygate::ui::internal
