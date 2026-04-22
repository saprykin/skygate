import QtQuick
import QtQuick.Controls

Item {
    id: overlayRoot
    readonly property var theme: skyContext.theme
    required property var sceneModel
    required property var interactionLayer
    required property var avoidItems
    readonly property var selectionMarkerData: sceneModel.selectionMarker

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

    Rectangle {
        id: hoverObjectLabel
        visible: interactionLayer.hoveredObjectLabel.length > 0
        x: Math.min(interactionLayer.hoverX + 14, Math.max(0, parent.width - width - 8))
        y: Math.min(interactionLayer.hoverY + 14, Math.max(0, parent.height - height - 8))
        radius: 6
        color: theme.overlayTooltipBackground
        border.width: 1
        border.color: theme.overlayTooltipBorder
        z: 10
        width: hoverLabelText.implicitWidth + 14
        height: hoverLabelText.implicitHeight + 8

        Label {
            id: hoverLabelText
            anchors.centerIn: parent
            text: interactionLayer.hoveredObjectLabel
            color: theme.overlayTooltipText
            font.family: "Avenir Next"
            font.pixelSize: 14
        }
    }

    Item {
        id: searchSelectionMarker
        visible: selectionMarkerData && selectionMarkerData.kind === "searchSelection"
        x: (selectionMarkerData && selectionMarkerData.x !== undefined)
            ? selectionMarkerData.x - (width * 0.5)
            : 0
        y: (selectionMarkerData && selectionMarkerData.y !== undefined)
            ? selectionMarkerData.y - (height * 0.5)
            : 0
        width: 34
        height: 34
        z: 12

        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            radius: width * 0.5
            color: theme.selectionMarkerFill
            border.width: 3
            border.color: theme.selectionMarkerBorder
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 10
            height: parent.height + 10
            radius: width * 0.5
            color: "transparent"
            border.width: 2
            border.color: theme.selectionMarkerOuterBorder
        }

        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 10
            radius: 5
            color: theme.selectionMarkerCenter
        }
    }

    Repeater {
        model: sceneModel.overlayItems

        delegate: Item {
            id: overlayDelegateRoot
            required property var modelData
            readonly property bool isCardinal: modelData.kind === "cardinal"
            readonly property color labelColor: modelData.color
                ? modelData.color
                : theme.overlayLabelText
            readonly property real labelWidth: overlayLabel.implicitWidth + 12
            readonly property real labelHeight: overlayLabel.implicitHeight + 6
            readonly property real labelX: modelData.x - (labelWidth * 0.5)
            readonly property real labelPreferredY: modelData.y - labelHeight - 8

            x: labelX
            y: isCardinal
                    ? overlayRoot.adjustedYToAvoidItems(
                        labelX,
                        labelPreferredY,
                        labelWidth,
                        labelHeight
                    )
                    : labelPreferredY
            width: labelWidth
            height: labelHeight
            visible: isCardinal
                    ? true
                    : !overlayRoot.overlapsAvoidItems(x, y, width, height)

            Rectangle {
                x: 0
                y: 0
                width: overlayDelegateRoot.labelWidth
                height: overlayDelegateRoot.labelHeight
                radius: overlayDelegateRoot.isCardinal ? 6 : 5
                color: overlayDelegateRoot.isCardinal
                    ? theme.overlayCardinalLabelBackground
                    : theme.overlayLabelBackground
                border.width: 1
                border.color: overlayDelegateRoot.labelColor
                z: overlayDelegateRoot.isCardinal ? 9 : 8

                Label {
                    id: overlayLabel
                    anchors.centerIn: parent
                    text: overlayDelegateRoot.modelData.text
                    color: overlayDelegateRoot.labelColor
                    font.family: "Avenir Next"
                    font.pixelSize: overlayDelegateRoot.isCardinal ? 14 : 11
                    font.bold: true
                }
            }
        }
    }
}
