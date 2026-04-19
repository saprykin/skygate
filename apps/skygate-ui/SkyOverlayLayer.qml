import QtQuick
import QtQuick.Controls

Item {
    id: overlayRoot
    required property var sceneModel
    required property var interactionLayer
    required property Item toolbarPanel
    required property Item toolbarToggle

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

    function overlapsTimelinePanel(itemX, itemY, itemWidth, itemHeight) {
        const margin = 4
        if (toolbarPanel.width > 1 && toolbarPanel.opacity > 0.05) {
            const toolbarRect = itemRect(toolbarPanel)
            if (
                overlayRoot.rectsOverlap(
                    itemX,
                    itemY,
                    itemWidth,
                    itemHeight,
                    toolbarRect.x - margin,
                    toolbarRect.y - margin,
                    toolbarRect.width + (margin * 2),
                    toolbarRect.height + (margin * 2)
                )
            ) {
                return true
            }
        }

        const toggleRect = itemRect(toolbarToggle)
        return toolbarToggle.visible
            && overlayRoot.rectsOverlap(
                itemX,
                itemY,
                itemWidth,
                itemHeight,
                toggleRect.x - margin,
                toggleRect.y - margin,
                toggleRect.width + (margin * 2),
                toggleRect.height + (margin * 2)
            )
    }

    function adjustedYToAvoidTimelinePanel(itemX, itemY, itemWidth, itemHeight) {
        let adjustedY = itemY
        const margin = 4

        if (toolbarPanel.width > 1 && toolbarPanel.opacity > 0.05) {
            const toolbarRect = itemRect(toolbarPanel)
            const toolbarX = toolbarRect.x - margin
            const toolbarY = toolbarRect.y - margin
            const toolbarWidth = toolbarRect.width + (margin * 2)
            const toolbarHeight = toolbarRect.height + (margin * 2)
            if (
                overlayRoot.rectsOverlap(
                    itemX,
                    adjustedY,
                    itemWidth,
                    itemHeight,
                    toolbarX,
                    toolbarY,
                    toolbarWidth,
                    toolbarHeight
                )
            ) {
                adjustedY = toolbarY + toolbarHeight
            }
        }

        const toggleRect = itemRect(toolbarToggle)
        const toggleX = toggleRect.x - margin
        const toggleY = toggleRect.y - margin
        const toggleWidth = toggleRect.width + (margin * 2)
        const toggleHeight = toggleRect.height + (margin * 2)
        if (
            toolbarToggle.visible
            && overlayRoot.rectsOverlap(
                itemX,
                adjustedY,
                itemWidth,
                itemHeight,
                toggleX,
                toggleY,
                toggleWidth,
                toggleHeight
            )
        ) {
            adjustedY = toggleY + toggleHeight
        }

        return adjustedY
    }

    Rectangle {
        id: hoverObjectLabel
        visible: interactionLayer.hoveredObjectLabel.length > 0
        x: Math.min(interactionLayer.hoverX + 14, Math.max(0, parent.width - width - 8))
        y: Math.min(interactionLayer.hoverY + 14, Math.max(0, parent.height - height - 8))
        radius: 6
        color: "#cc0a1222"
        border.width: 1
        border.color: "#6b8fc8"
        z: 10
        width: hoverLabelText.implicitWidth + 14
        height: hoverLabelText.implicitHeight + 8

        Label {
            id: hoverLabelText
            anchors.centerIn: parent
            text: interactionLayer.hoveredObjectLabel
            color: "#e6f0ff"
            font.family: "Avenir Next"
            font.pixelSize: 14
        }
    }

    Repeater {
        model: sceneModel.overlayItems

        delegate: Rectangle {
            required property var modelData
            readonly property bool isCardinal: modelData.kind === "cardinal"
            readonly property color labelColor: modelData.color ? modelData.color : "#c9dcff"
            readonly property real labelX: modelData.x - (width * 0.5)
            readonly property real preferredY: modelData.y - height - 8

            x: labelX
            y: isCardinal
                ? overlayRoot.adjustedYToAvoidTimelinePanel(labelX, preferredY, width, height)
                : preferredY
            visible: isCardinal
                ? true
                : !overlayRoot.overlapsTimelinePanel(x, y, width, height)
            radius: isCardinal ? 6 : 5
            color: isCardinal ? "#880a1222" : "#aa071328"
            border.width: 1
            border.color: labelColor
            z: isCardinal ? 9 : 8
            width: overlayLabel.implicitWidth + 12
            height: overlayLabel.implicitHeight + 6

            Label {
                id: overlayLabel
                anchors.centerIn: parent
                text: modelData.text
                color: labelColor
                font.family: "Avenir Next"
                font.pixelSize: isCardinal ? 14 : 11
                font.bold: true
            }
        }
    }
}
