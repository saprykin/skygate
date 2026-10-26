#include "SkyReferenceOverlayLabelBuilder.hpp"
#include "SkyReferenceOverlayLayers.hpp"
#include "math/Geometry2d.hpp"

#include <QColor>
#include <QPointF>
#include <QString>

#include <limits>
#include <optional>

namespace {

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

    for (int index = 0; index < skygate::ui::internal::SkyReferenceOverlayLayers::kSampleCount; ++index) {
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
    for (const auto& layer :
         skygate::ui::internal::SkyReferenceOverlayLayers::layers(renderTheme, overlayLayers, skyContext.observer)) {
        if (!layer.enabled) {
            continue;
        }

        appendReferenceLayerLabel(
            overlayItems, preparedProjection, layer.label, layer.color, [&layer, &skyContext](const int index) {
                return skygate::ui::internal::SkyReferenceOverlayLayers::samplePoint(
                    layer.circle, index, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }
}

}  // namespace skygate::ui::internal
