#include "SkyReferenceOverlayLabelBuilder.hpp"
#include "CelestialReferenceCalculator.hpp"
#include "math/Geometry2d.hpp"

#include <QColor>
#include <QPointF>
#include <QString>

#include <limits>
#include <optional>

namespace {

constexpr int kReferenceLabelSampleCount = skygate::ephemeris::CelestialReferenceCalculator::kReferenceCircleSamples;
constexpr double kReferenceLabelEdgeMarginPx = 36.0;

template <typename CoordinateAt>
void appendReferenceLayerLabel(
    std::vector<SkyOverlayItem>& overlayItems,
    const skygate::core::PreparedProjection& preparedProjection,
    const QString& text,
    const QColor& color,
    CoordinateAt coordinateAt
)
{
    const auto& params = preparedProjection.params();
    const double targetX = params.viewportWidth * 0.72;
    const double targetY = params.viewportHeight * 0.34;

    std::optional<QPointF> bestPoint;
    double bestScore = std::numeric_limits<double>::max();
    std::optional<QPointF> fallbackPoint;
    double fallbackScore = std::numeric_limits<double>::max();

    for (int index = 0; index < kReferenceLabelSampleCount; ++index) {
        const auto projected = preparedProjection.project(coordinateAt(index));
        if (!projected.isVisible()) {
            continue;
        }

        const double score = skygate::core::Geometry2d::squaredDistance2d(projected.x, projected.y, targetX, targetY);
        if (score < fallbackScore) {
            fallbackPoint = QPointF(projected.x, projected.y);
            fallbackScore = score;
        }
        if (skygate::core::Geometry2d::pointWithinMargin(
                projected.x, projected.y, params.viewportWidth, params.viewportHeight, kReferenceLabelEdgeMarginPx
            )
            && score < bestScore) {
            bestPoint = QPointF(projected.x, projected.y);
            bestScore = score;
        }
    }

    const std::optional<QPointF>& labelPoint = bestPoint.has_value() ? bestPoint : fallbackPoint;
    if (!labelPoint.has_value()) {
        return;
    }

    overlayItems.push_back(
        SkyOverlayItem{
            .kind = "referenceLine", .x = labelPoint->x(), .y = labelPoint->y(), .text = text, .color = color
        }
    );
}

}  // namespace

namespace skygate::ui::internal {

void SkyReferenceOverlayLabelBuilder::appendLabels(
    std::vector<SkyOverlayItem>& overlayItems,
    const skygate::core::PreparedProjection& preparedProjection,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers,
    const skygate::core::ObservationContext& skyContext
)
{
    if (overlayLayers.ecliptic) {
        appendReferenceLayerLabel(
            overlayItems, preparedProjection, "Ecliptic", renderTheme.eclipticLine, [&skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(
                    index, kReferenceLabelSampleCount, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.celestialEquator) {
        appendReferenceLayerLabel(
            overlayItems,
            preparedProjection,
            "Celestial equator",
            renderTheme.celestialEquatorLine,
            [&skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index, kReferenceLabelSampleCount, 0.0, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.circumpolarBoundary && skyContext.observer.isValid()) {
        const double boundaryDeclinationDeg =
            skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(skyContext.observer);
        appendReferenceLayerLabel(
            overlayItems,
            preparedProjection,
            "Circumpolar",
            renderTheme.circumpolarBoundaryLine,
            [boundaryDeclinationDeg, &skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index, kReferenceLabelSampleCount, boundaryDeclinationDeg, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }
}

}  // namespace skygate::ui::internal
