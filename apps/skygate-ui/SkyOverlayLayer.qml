import QtQuick
pragma ComponentBehavior: Bound

Item {
    id: overlayRoot
    objectName: "skyOverlayLayer"
    readonly property var skyContextController: skyContext
    readonly property var theme: skyContextController.theme
    required property var sceneModel
    required property var interactionLayer
    required property var avoidItems
    readonly property var selectionMarkerData: sceneModel.selectionMarker
    readonly property var inspectorData: sceneModel.selectedObjectInspector
    readonly property bool inspectorIsTracked: inspectorData
        && inspectorData.targetKind !== undefined
        && inspectorData.targetId !== undefined
        && skyContextController.hasTrackedTarget
        && skyContextController.trackedTargetKind === inspectorData.targetKind
        && skyContextController.trackedTargetId === inspectorData.targetId

    function rectsOverlap(firstX, firstY, firstWidth, firstHeight, secondX, secondY, secondWidth, secondHeight) {
        const firstRight = firstX + firstWidth
        const firstBottom = firstY + firstHeight
        const secondRight = secondX + secondWidth
        const secondBottom = secondY + secondHeight
        return firstX < secondRight
            && firstRight > secondX
            && firstY < secondBottom
            && firstBottom > secondY
    }

    function itemRect(item) {
        const topLeft = item.mapToItem(overlayRoot, 0, 0)
        return {
            x: topLeft.x,
            y: topLeft.y,
            width: item.width,
            height: item.height
        }
    }

    function avoidItemRects() {
        const margin = 4
        const rects = []
        if (!avoidItems) {
            return rects
        }

        for (let index = 0; index < avoidItems.length; ++index) {
            const item = avoidItems[index]
            if (!item || !item.visible || item.width <= 1 || item.height <= 1) {
                continue
            }

            const opacity = item.opacity !== undefined ? item.opacity : 1.0
            if (opacity <= 0.05) {
                continue
            }

            const rect = itemRect(item)
            rects.push({
                x: rect.x - margin,
                y: rect.y - margin,
                width: rect.width + (margin * 2),
                height: rect.height + (margin * 2)
            })
        }

        rects.sort(function(lhs, rhs) { return lhs.y - rhs.y })
        return rects
    }

    function overlapsAvoidItems(itemX, itemY, itemWidth, itemHeight) {
        const rects = avoidItemRects()
        for (let index = 0; index < rects.length; ++index) {
            const avoidRect = rects[index]
            if (
                overlayRoot.rectsOverlap(
                    itemX,
                    itemY,
                    itemWidth,
                    itemHeight,
                    avoidRect.x,
                    avoidRect.y,
                    avoidRect.width,
                    avoidRect.height
                )
            ) {
                return true
            }
        }

        return false
    }

    function adjustedYToAvoidItems(itemX, itemY, itemWidth, itemHeight) {
        let adjustedY = itemY
        const rects = avoidItemRects()
        let changed = true
        let iteration = 0

        while (changed && iteration <= rects.length) {
            changed = false
            ++iteration
            for (let index = 0; index < rects.length; ++index) {
                const avoidRect = rects[index]
                if (
                    overlayRoot.rectsOverlap(
                        itemX,
                        adjustedY,
                        itemWidth,
                        itemHeight,
                        avoidRect.x,
                        avoidRect.y,
                        avoidRect.width,
                        avoidRect.height
                    )
                ) {
                    adjustedY = avoidRect.y + avoidRect.height
                    changed = true
                }
            }
        }

        return adjustedY
    }

    SkyHoverLabel {
        theme: overlayRoot.theme
        interactionLayer: overlayRoot.interactionLayer
    }

    SkySelectionMarker {
        theme: overlayRoot.theme
        markerData: overlayRoot.selectionMarkerData
    }

    SkyObjectInspector {
        theme: overlayRoot.theme
        skyContextController: overlayRoot.skyContextController
        sceneModel: overlayRoot.sceneModel
        inspectorData: overlayRoot.inspectorData
        inspectorIsTracked: overlayRoot.inspectorIsTracked
        overlayLayer: overlayRoot
    }

    Repeater {
        model: overlayRoot.sceneModel.overlayItems

        delegate: SkyOverlayLabel {
            theme: overlayRoot.theme
            avoidance: overlayRoot
        }
    }
}
