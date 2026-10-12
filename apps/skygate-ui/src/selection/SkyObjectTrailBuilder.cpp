#include "SkyObjectTrailBuilder.hpp"
#include "SkyObjectTrailInput.hpp"
#include "SkyPerformanceLogging.hpp"
#include "SkyRenderLabelBuilder.hpp"
#include "SkyRenderLine.hpp"
#include "math/LinePattern.hpp"
#include "math/ProjectedPolylineBuilder.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QString>

#include <algorithm>
#include <array>

namespace {

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
    frame.lines.push_back(SkyRenderLine{.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2, .widthPx = widthPx, .color = color});
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
             skygate::core::LineSegment2d{.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2},
             kObjectTrailPastDashLengthPx,
             kObjectTrailPastDashGapPx
         )) {
        appendTrailLine(frame, dash.x1, dash.y1, dash.x2, dash.y2, widthPx, color);
    }
}

void appendFutureTrailTick(
    SkyRenderFrame& frame, const skygate::core::ScreenPoint& point, const int offsetMinutes, const QColor& color
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

    skygate::ui::internal::SkyRenderLabelBuilder::append(
        frame.labels, "trailTick", point.x, point.y, QString("+%1h").arg(offsetMinutes / 60), color
    );
}

}  // namespace

void SkyObjectTrailBuilder::appendTrail(SkyRenderFrame& frame, const SkyObjectTrailInput& input) const
{
    QElapsedTimer timer;
    skygate::ui::SkyPerformanceLogging::startTimer(timer);

    const skygate::core::ObservationContext& context =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    if (input.ephemerisEngine == nullptr || input.preparedProjection == nullptr || !context.observer.isValid()) {
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

    const SkyObjectTrailSamplingInput samplingInput{
        .ephemerisEngine = input.ephemerisEngine,
        .targetBody = input.targetBody,
        .targetState = input.targetState,
        .skyContext = input.skyContext,
        .ephemerisRequest = input.ephemerisRequest,
        .targetBodyIndex = input.targetBodyIndex,
        .guidanceStrategy = input.guidanceStrategy.get()
    };
    const bool sampleCacheHit = m_trailSampler.hasCachedSamples(samplingInput);
    const auto& samples = m_trailSampler.samples(samplingInput);
    const qint64 sampleNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);

    int lineCountBefore = 0;
    int labelCountBefore = 0;
    if (skygate::ui::SkyPerformanceLogging::enabled()) {
        lineCountBefore = static_cast<int>(frame.lines.size());
        labelCountBefore = static_cast<int>(frame.labels.size());
    }

    for (const auto& sample : samples) {
        if (!sample.horizontal.has_value() || !sample.horizontal->isValid()) {
            hasPreviousCoordinate = false;
            continue;
        }

        if (hasPreviousCoordinate) {
            const std::array<skygate::core::HorizontalCoordinate, 2> coordinates{
                previousCoordinate, *sample.horizontal
            };
            const bool isPastSegment = previousOffsetMinutes < 0 && sample.offsetMinutes <= 0;
            for (const auto& segment :
                 polylineBuilder.build(*input.preparedProjection, coordinates, maxSegmentLengthSquared)) {
                if (isPastSegment) {
                    appendDashedTrailLine(
                        frame, segment.x1, segment.y1, segment.x2, segment.y2, kObjectTrailPastWidthPx, pastColor
                    );
                } else {
                    appendTrailLine(
                        frame, segment.x1, segment.y1, segment.x2, segment.y2, kObjectTrailFutureWidthPx, futureColor
                    );
                }
            }
        }

        const auto projected = input.preparedProjection->project(*sample.horizontal);
        if (projected.isVisible && projected.isFinite() && sample.offsetMinutes > 0
            && sample.offsetMinutes % (kObjectTrailFutureTickStepHours * 60) == 0) {
            appendFutureTrailTick(frame, projected, sample.offsetMinutes, futureColor);
        }

        previousCoordinate = *sample.horizontal;
        previousOffsetMinutes = sample.offsetMinutes;
        hasPreviousCoordinate = true;
    }

    if (skygate::ui::SkyPerformanceLogging::enabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "trail append elapsedMs=" << skygate::ui::SkyPerformanceLogging::elapsedMilliseconds(timer)
            << "sampleMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(sampleNs)
            << "renderMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(timer.nsecsElapsed() - sampleNs)
            << "samples=" << static_cast<qsizetype>(samples.size()) << "cacheHit=" << sampleCacheHit
            << "highPrecision=" << SkyObjectTrailSampler::usesAdaptiveTrailSampling(samplingInput)
            << "guidance=" << SkyObjectTrailSampler::shouldUseGuidanceTrail(samplingInput)
            << "fixedEquatorial=" << SkyObjectTrailSampler::isFixedEquatorialTrailTarget(samplingInput)
            << "bodyIndex=" << static_cast<qulonglong>(input.targetBodyIndex)
            << "linesAdded=" << static_cast<int>(frame.lines.size()) - lineCountBefore
            << "labelsAdded=" << static_cast<int>(frame.labels.size()) - labelCountBefore;
    }
}
