#include "SkyObjectTrailBuilder.hpp"

#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/core/math/LinePattern.hpp"
#include "skygate/core/math/ProjectedPolylineBuilder.hpp"
#include "skygate/ephemeris/BodyTrailCalculator.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <QColor>
#include <QVariantMap>

#include <algorithm>
#include <array>

namespace {

constexpr int kObjectTrailPastHours = 6;
constexpr int kObjectTrailFutureHours = 18;
constexpr int kObjectTrailSampleStepMinutes = 30;
constexpr double kObjectTrailPastWidthPx = 1.4;
constexpr double kObjectTrailFutureWidthPx = 2.0;
constexpr double kObjectTrailPastDashLengthPx = 8.0;
constexpr double kObjectTrailPastDashGapPx = 7.0;
constexpr int kObjectTrailFutureTickStepHours = 6;
constexpr double kObjectTrailFutureTickRadiusPx = 5.0;

QColor colorWithAlpha(const QColor& color, const int alpha)
{
    QColor adjusted = color;
    adjusted.setAlpha(alpha);
    return adjusted;
}

void appendTrailLine(
    SkyRenderFrame& frame,
    const double x1,
    const double y1,
    const double x2,
    const double y2,
    const double widthPx,
    const QColor& color
)
{
    frame.lines.push_back(SkyRenderLine {
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .widthPx = widthPx,
        .color = color
    });
}

void appendDashedTrailLine(
    SkyRenderFrame& frame,
    const double x1,
    const double y1,
    const double x2,
    const double y2,
    const double widthPx,
    const QColor& color
)
{
    const skygate::core::DashedLineBuilder dashBuilder;
    for (const auto& dash : dashBuilder.build(
             skygate::core::LineSegment2d {
                 .x1 = x1,
                 .y1 = y1,
                 .x2 = x2,
                 .y2 = y2
             },
             kObjectTrailPastDashLengthPx,
             kObjectTrailPastDashGapPx
         )) {
        appendTrailLine(
            frame,
            dash.x1,
            dash.y1,
            dash.x2,
            dash.y2,
            widthPx,
            color
        );
    }
}

void appendFutureTrailTick(
    SkyRenderFrame& frame,
    const skygate::core::ScreenPoint& point,
    const int offsetMinutes,
    const QColor& color
)
{
    appendTrailLine(
        frame,
        point.x - kObjectTrailFutureTickRadiusPx,
        point.y,
        point.x + kObjectTrailFutureTickRadiusPx,
        point.y,
        kObjectTrailFutureWidthPx,
        color
    );
    appendTrailLine(
        frame,
        point.x,
        point.y - kObjectTrailFutureTickRadiusPx,
        point.x,
        point.y + kObjectTrailFutureTickRadiusPx,
        kObjectTrailFutureWidthPx,
        color
    );

    QVariantMap entry;
    entry.insert("kind", "trailTick");
    entry.insert("x", point.x);
    entry.insert("y", point.y);
    entry.insert("text", QString("+%1h").arg(offsetMinutes / 60));
    entry.insert("color", color);
    frame.labels.push_back(entry);
}

}  // namespace

void SkyObjectTrailBuilder::appendTrail(
    SkyRenderFrame& frame,
    const SkyObjectTrailInput& input
) const
{
    if (
        input.ephemerisEngine == nullptr
        || input.preparedProjection == nullptr
        || !input.skyContext.observer.isValid()
    ) {
        return;
    }

    const double maxSegmentLength = std::max(input.viewportWidth, input.viewportHeight) * 0.35;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;
    const QColor pastColor = colorWithAlpha(input.renderTheme.selectionMarkerBorder, 105);
    const QColor futureColor = colorWithAlpha(input.renderTheme.selectionMarkerBorder, 175);
    const skygate::core::ProjectedPolylineBuilder polylineBuilder;

    bool hasPreviousCoordinate = false;
    skygate::core::HorizontalCoordinate previousCoordinate;
    int previousOffsetMinutes = 0;

    const skygate::ephemeris::BodyTrailCalculator trailCalculator;
    const auto samples = trailCalculator.sample(
        *input.ephemerisEngine,
        input.skyContext,
        input.targetBodyIndex,
        skygate::ephemeris::BodyTrailOptions {
            .pastHours = kObjectTrailPastHours,
            .futureHours = kObjectTrailFutureHours,
            .sampleStepMinutes = kObjectTrailSampleStepMinutes
        }
    );

    for (const auto& sample : samples) {
        if (!sample.horizontal.has_value() || !sample.horizontal->isValid()) {
            hasPreviousCoordinate = false;
            continue;
        }

        if (hasPreviousCoordinate) {
            const std::array<skygate::core::HorizontalCoordinate, 2> coordinates {
                previousCoordinate,
                *sample.horizontal
            };
            const bool isPastSegment = previousOffsetMinutes < 0 && sample.offsetMinutes <= 0;
            for (const auto& segment : polylineBuilder.build(
                     *input.preparedProjection,
                     coordinates,
                     maxSegmentLengthSquared
                 )) {
                if (isPastSegment) {
                    appendDashedTrailLine(
                        frame,
                        segment.x1,
                        segment.y1,
                        segment.x2,
                        segment.y2,
                        kObjectTrailPastWidthPx,
                        pastColor
                    );
                } else {
                    appendTrailLine(
                        frame,
                        segment.x1,
                        segment.y1,
                        segment.x2,
                        segment.y2,
                        kObjectTrailFutureWidthPx,
                        futureColor
                    );
                }
            }
        }

        const auto projected = input.preparedProjection->project(*sample.horizontal);
        if (
            projected.isVisible
            && projected.isFinite()
            && sample.offsetMinutes > 0
            && sample.offsetMinutes % (kObjectTrailFutureTickStepHours * 60) == 0
        ) {
            appendFutureTrailTick(frame, projected, sample.offsetMinutes, futureColor);
        }

        previousCoordinate = *sample.horizontal;
        previousOffsetMinutes = sample.offsetMinutes;
        hasPreviousCoordinate = true;
    }
}
