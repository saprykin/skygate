#include "SkyObjectTrailBuilder.hpp"

#include "SkySceneShared.hpp"

#include "skygate/core/math/GeometryMath.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <QColor>
#include <QVariantMap>

#include <algorithm>
#include <chrono>
#include <cmath>

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
    const double deltaX = x2 - x1;
    const double deltaY = y2 - y1;
    const double length = std::hypot(deltaX, deltaY);
    if (length <= 1e-9) {
        return;
    }

    const double unitX = deltaX / length;
    const double unitY = deltaY / length;
    double dashStart = 0.0;
    while (dashStart < length) {
        const double dashEnd = std::min(dashStart + kObjectTrailPastDashLengthPx, length);
        appendTrailLine(
            frame,
            x1 + (unitX * dashStart),
            y1 + (unitY * dashStart),
            x1 + (unitX * dashEnd),
            y1 + (unitY * dashEnd),
            widthPx,
            color
        );
        dashStart += kObjectTrailPastDashLengthPx + kObjectTrailPastDashGapPx;
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

    bool hasPreviousPoint = false;
    skygate::core::ScreenPoint previousPoint;
    int previousOffsetMinutes = 0;
    const int pastMinutes = kObjectTrailPastHours * 60;
    const int futureMinutes = kObjectTrailFutureHours * 60;

    for (
        int offsetMinutes = -pastMinutes;
        offsetMinutes <= futureMinutes;
        offsetMinutes += kObjectTrailSampleStepMinutes
    ) {
        skygate::core::SkyContext sampleContext = input.skyContext;
        sampleContext.utcTime = input.skyContext.utcTime + std::chrono::minutes(offsetMinutes);
        const auto state = input.ephemerisEngine->computeBodyState(
            sampleContext,
            input.targetBodyIndex
        );
        if (!state.has_value() || !skySceneHorizontalIsFinite(state->horizontal)) {
            hasPreviousPoint = false;
            continue;
        }

        const auto projected = input.preparedProjection->project(state->horizontal);
        if (!skySceneScreenPointIsFinite(projected)) {
            hasPreviousPoint = false;
            continue;
        }

        if (hasPreviousPoint) {
            const double lengthSquared = skygate::core::GeometryMath::squaredDistance2d(
                projected.x,
                projected.y,
                previousPoint.x,
                previousPoint.y
            );
            if (lengthSquared <= maxSegmentLengthSquared) {
                const bool isPastSegment = previousOffsetMinutes < 0 && offsetMinutes <= 0;
                if (isPastSegment) {
                    appendDashedTrailLine(
                        frame,
                        previousPoint.x,
                        previousPoint.y,
                        projected.x,
                        projected.y,
                        kObjectTrailPastWidthPx,
                        pastColor
                    );
                } else {
                    appendTrailLine(
                        frame,
                        previousPoint.x,
                        previousPoint.y,
                        projected.x,
                        projected.y,
                        kObjectTrailFutureWidthPx,
                        futureColor
                    );
                }
            }
        }

        if (
            offsetMinutes > 0
            && offsetMinutes % (kObjectTrailFutureTickStepHours * 60) == 0
        ) {
            appendFutureTrailTick(frame, projected, offsetMinutes, futureColor);
        }

        previousPoint = projected;
        previousOffsetMinutes = offsetMinutes;
        hasPreviousPoint = true;
    }
}
