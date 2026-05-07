#pragma once

#include "QmlTestSupport.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QColor>
#include <QDateTime>
#include <QList>
#include <QRect>
#include <QTimeZone>

using namespace skygate::ui::tests;

namespace {

QObject* renderingWindowWithObjectName(QObject* root, const QString& objectName)
{
    for (QObject* object : objectTree(root)) {
        if (qobject_cast<QWindow*>(object) != nullptr && object->objectName() == objectName) {
            return object;
        }
    }
    return nullptr;
}

bool renderingTriggerMenuItem(QObject* menuItem)
{
    if (menuItem == nullptr) {
        return false;
    }
    if (menuItem->metaObject()->indexOfMethod("trigger()") >= 0) {
        return QMetaObject::invokeMethod(menuItem, "trigger");
    }
    if (menuItem->metaObject()->indexOfSignal("triggered()") >= 0) {
        return QMetaObject::invokeMethod(menuItem, "triggered");
    }
    return false;
}

int renderingMaxRgbChannelDistance(const QColor& lhs, const QColor& rhs)
{
    return std::max({
        std::abs(lhs.red() - rhs.red()),
        std::abs(lhs.green() - rhs.green()),
        std::abs(lhs.blue() - rhs.blue()),
    });
}

bool renderingColorsAreNear(
    const QColor& lhs,
    const QColor& rhs,
    const int maxChannelDistance
)
{
    return renderingMaxRgbChannelDistance(lhs, rhs) <= maxChannelDistance;
}

QRectF renderingQuickItemSceneRect(const QQuickItem& item)
{
    return QRectF(
        item.mapToScene(QPointF(0.0, 0.0)),
        item.mapToScene(QPointF(item.width(), item.height()))
    ).normalized();
}

QRect renderingImageRectForSceneRect(
    const QQuickWindow& window,
    const QImage& image,
    const QRectF& sceneRect
)
{
    if (window.width() <= 0 || window.height() <= 0 || image.isNull()) {
        return {};
    }

    const double scaleX = static_cast<double>(image.width()) / static_cast<double>(window.width());
    const double scaleY = static_cast<double>(image.height()) / static_cast<double>(window.height());
    const int left = static_cast<int>(std::floor(sceneRect.left() * scaleX));
    const int top = static_cast<int>(std::floor(sceneRect.top() * scaleY));
    const int right = static_cast<int>(std::ceil(sceneRect.right() * scaleX));
    const int bottom = static_cast<int>(std::ceil(sceneRect.bottom() * scaleY));
    return QRect(QPoint(left, top), QPoint(right, bottom))
        .normalized()
        .intersected(QRect(0, 0, image.width(), image.height()));
}

QPoint renderingImagePointForScenePoint(
    const QQuickWindow& window,
    const QImage& image,
    const QPointF& scenePoint
)
{
    if (window.width() <= 0 || window.height() <= 0 || image.isNull()) {
        return {};
    }

    const double scaleX = static_cast<double>(image.width()) / static_cast<double>(window.width());
    const double scaleY = static_cast<double>(image.height()) / static_cast<double>(window.height());
    return QPoint(
        qBound(0, static_cast<int>(std::round(scenePoint.x() * scaleX)), image.width() - 1),
        qBound(0, static_cast<int>(std::round(scenePoint.y() * scaleY)), image.height() - 1)
    );
}

bool renderingImageRegionContainsColorNear(
    const QImage& image,
    const QRect& imageRect,
    const QColor& targetColor,
    const int maxChannelDistance
)
{
    if (image.isNull() || imageRect.isEmpty()) {
        return false;
    }

    for (int y = imageRect.top(); y <= imageRect.bottom(); ++y) {
        for (int x = imageRect.left(); x <= imageRect.right(); ++x) {
            if (
                renderingColorsAreNear(
                    image.pixelColor(x, y),
                    targetColor,
                    maxChannelDistance
                )
            ) {
                return true;
            }
        }
    }
    return false;
}

bool renderingItemRegionContainsColorNear(
    const QQuickWindow& window,
    const QImage& image,
    const QQuickItem& item,
    const QColor& targetColor,
    const int maxChannelDistance
)
{
    return renderingImageRegionContainsColorNear(
        image,
        renderingImageRectForSceneRect(window, image, renderingQuickItemSceneRect(item)),
        targetColor,
        maxChannelDistance
    );
}

bool renderingScenePointIsColorNear(
    const QQuickWindow& window,
    const QImage& image,
    const QPointF& scenePoint,
    const QColor& targetColor,
    const int maxChannelDistance
)
{
    if (image.isNull()) {
        return false;
    }

    const QPoint imagePoint = renderingImagePointForScenePoint(window, image, scenePoint);
    return renderingColorsAreNear(
        image.pixelColor(imagePoint),
        targetColor,
        maxChannelDistance
    );
}

QList<QColor> renderingExpectedGeometryColors(
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme,
    const QString& propertyName
)
{
    if (propertyName == QStringLiteral("horizon")) {
        return {renderTheme.horizonLine};
    }
    if (propertyName == QStringLiteral("altAzGrid")) {
        return {renderTheme.gridAltitudeLine, renderTheme.gridAzimuthLine};
    }
    if (propertyName == QStringLiteral("ecliptic")) {
        return {renderTheme.eclipticLine};
    }
    if (propertyName == QStringLiteral("celestialEquator")) {
        return {renderTheme.celestialEquatorLine};
    }
    if (propertyName == QStringLiteral("circumpolarBoundary")) {
        return {renderTheme.circumpolarBoundaryLine};
    }
    if (propertyName == QStringLiteral("deepSkyObjects")) {
        return {renderTheme.bodyDeepSkyObject};
    }
    return {};
}

bool renderingHasVisibleGeometryWithColor(
    const QSGNode* paintNode,
    const QColor& color,
    const QSizeF& viewportSize,
    QString* failure
)
{
    const SkyViewGeometryProbe probe = geometryMaterialColorProbe(paintNode, color);
    if (!probe.hasGeometry()) {
        if (failure != nullptr) {
            *failure = QStringLiteral("No scene-graph geometry used color %1; actual colors: %2")
                .arg(color.name(QColor::HexArgb), geometryMaterialColorNames(paintNode).join(", "));
        }
        return false;
    }

    if (!geometryProbeIntersectsViewport(probe, viewportSize)) {
        if (failure != nullptr) {
            *failure = QStringLiteral("Scene-graph geometry for %1 at [%2,%3 %4x%5] misses %6x%7")
                .arg(color.name(QColor::HexArgb))
                .arg(probe.bounds.x())
                .arg(probe.bounds.y())
                .arg(probe.bounds.width())
                .arg(probe.bounds.height())
                .arg(viewportSize.width())
                .arg(viewportSize.height());
        }
        return false;
    }
    return true;
}

bool windowHasMultipleSampledColors(QQuickWindow& window)
{
    const QImage image = window.grabWindow();
    if (image.isNull()) {
        return false;
    }

    std::optional<QRgb> firstColor;
    for (int y = 0; y < image.height(); y += std::max(1, image.height() / 12)) {
        for (int x = 0; x < image.width(); x += std::max(1, image.width() / 12)) {
            const QRgb color = image.pixel(x, y);
            if (!firstColor.has_value()) {
                firstColor = color;
                continue;
            }
            if (color != firstColor.value()) {
                return true;
            }
        }
    }
    return false;
}

bool visibleQuickItemsFitWithinWindow(QQuickWindow& window, QString* failure)
{
    const QRectF windowRect(0.0, 0.0, window.width(), window.height());
    for (QObject* object : objectTree(&window)) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (
            item == nullptr
            || item->window() != &window
            || !item->isVisible()
            || item->width() <= 0.0
            || item->height() <= 0.0
        ) {
            continue;
        }

        const QRectF bounds(item->mapToScene(QPointF(0.0, 0.0)), item->size());
        const QRectF relaxedWindowRect = windowRect.adjusted(-1.0, -1.0, 1.0, 1.0);
        if (!relaxedWindowRect.contains(bounds)) {
            if (failure != nullptr) {
                *failure = QStringLiteral("%1 at [%2,%3 %4x%5] outside %6x%7")
                    .arg(QString::fromUtf8(item->metaObject()->className()))
                    .arg(bounds.x())
                    .arg(bounds.y())
                    .arg(bounds.width())
                    .arg(bounds.height())
                    .arg(window.width())
                    .arg(window.height());
            }
            return false;
        }
    }
    return true;
}

}  // namespace
