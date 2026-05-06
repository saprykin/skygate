import QtQuick
import QtQuick.Controls

Item {
    id: overlayRoot
    objectName: "skyOverlayLayer"
    readonly property var theme: skyContext.theme
    required property var sceneModel
    required property var interactionLayer
    required property var avoidItems
    readonly property var selectionMarkerData: sceneModel.selectionMarker
    readonly property var inspectorData: sceneModel.selectedObjectInspector
    readonly property bool inspectorIsTracked: inspectorData
        && inspectorData.targetKind !== undefined
        && inspectorData.targetId !== undefined
        && skyContext.hasTrackedTarget
        && skyContext.trackedTargetKind === inspectorData.targetKind
        && skyContext.trackedTargetId === inspectorData.targetId

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
        objectName: "hoverObjectLabel"
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
        objectName: "searchSelectionMarker"
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

    Rectangle {
        id: objectInspector
        objectName: "objectInspector"
        readonly property bool hasInspector: inspectorData
            && inspectorData.visible === true
            && inspectorData.title !== undefined
        readonly property bool inspectorPinned: objectInspector.hasInspector
            && inspectorData.pinned === true
        property bool draggingInspector: false
        property real dragStartPointerX: 0
        property real dragStartPointerY: 0
        property real dragStartPanelX: 0
        property real dragStartPanelY: 0

        function clampedInspectorX(value) {
            return Math.min(
                Math.max(8, value),
                Math.max(8, overlayRoot.width - objectInspector.width - 8)
            )
        }

        function clampedInspectorY(value) {
            return Math.min(
                Math.max(8, value),
                Math.max(8, overlayRoot.height - objectInspector.height - 8)
            )
        }

        function pointerPositionInOverlay(mouse) {
            return objectInspector.mapToItem(overlayRoot, mouse.x, mouse.y)
        }

        visible: hasInspector
        width: Math.min(350, Math.max(280, inspectorContent.implicitWidth + 22))
        height: inspectorContent.implicitHeight + 18
        x: hasInspector
            ? clampedInspectorX(inspectorData.x)
            : 0
        y: hasInspector
            ? clampedInspectorY(inspectorData.y)
            : 0
        radius: 8
        color: theme.toolbarDropdownBackground
        border.width: 1
        border.color: theme.toolbarDropdownBorder
        z: 14
        clip: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            preventStealing: true
            cursorShape: objectInspector.draggingInspector
                ? Qt.ClosedHandCursor
                : Qt.OpenHandCursor
            onPressed: function(mouse) {
                if (mouse.button !== Qt.LeftButton || !objectInspector.hasInspector) {
                    return
                }

                const pointer = objectInspector.pointerPositionInOverlay(mouse)
                objectInspector.draggingInspector = true
                objectInspector.dragStartPointerX = pointer.x
                objectInspector.dragStartPointerY = pointer.y
                objectInspector.dragStartPanelX = objectInspector.x
                objectInspector.dragStartPanelY = objectInspector.y
            }
            onPositionChanged: function(mouse) {
                if (!objectInspector.draggingInspector) {
                    return
                }

                const pointer = objectInspector.pointerPositionInOverlay(mouse)
                const nextX = objectInspector.clampedInspectorX(
                    objectInspector.dragStartPanelX + pointer.x - objectInspector.dragStartPointerX
                )
                const nextY = objectInspector.clampedInspectorY(
                    objectInspector.dragStartPanelY + pointer.y - objectInspector.dragStartPointerY
                )
                sceneModel.moveSelectedObjectInspector(nextX, nextY)
            }
            onReleased: function() {
                objectInspector.draggingInspector = false
            }
            onCanceled: {
                objectInspector.draggingInspector = false
            }
            onWheel: function(wheel) {
                wheel.accepted = true
            }
        }

        Column {
            id: inspectorContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 7

            Row {
                width: parent.width
                height: Math.max(
                    inspectorTitle.implicitHeight,
                    centerInspectorButton.height,
                    trackInspectorButton.height,
                    pinInspectorButton.height,
                    closeInspectorButton.height
                )
                spacing: 8

                Text {
                    id: inspectorTitle
                    width: parent.width
                        - centerInspectorButton.width
                        - trackInspectorButton.width
                        - pinInspectorButton.width
                        - closeInspectorButton.width
                        - (parent.spacing * 4)
                    text: objectInspector.hasInspector ? inspectorData.title : ""
                    color: theme.toolbarPrimaryText
                    font.family: "Avenir Next"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    height: parent.height
                }

                ToolButton {
                    id: centerInspectorButton
                    objectName: "objectInspectorCenterButton"
                    width: 64
                    height: 22
                    text: "Center"
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    enabled: objectInspector.hasInspector
                        && inspectorData.targetKind !== undefined
                        && inspectorData.targetId !== undefined
                    onClicked: {
                        skyContext.focusSearchTarget(inspectorData.targetKind, inspectorData.targetId)
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Center view on object"

                    contentItem: Text {
                        text: centerInspectorButton.text
                        color: centerInspectorButton.enabled
                            ? theme.toolbarButtonText
                            : theme.toolbarSecondaryText
                        font: centerInspectorButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: 6
                        color: centerInspectorButton.down
                            ? theme.toolbarButtonBackgroundPressed
                            : (centerInspectorButton.hovered
                                   ? theme.toolbarButtonBackgroundHover
                                   : theme.toolbarButtonBackground)
                        border.width: 1
                        border.color: theme.toolbarButtonBorder
                    }
                }

                ToolButton {
                    id: trackInspectorButton
                    objectName: overlayRoot.inspectorIsTracked
                        ? "objectInspectorUntrackButton"
                        : "objectInspectorTrackButton"
                    width: 64
                    height: 22
                    text: overlayRoot.inspectorIsTracked ? "Untrack" : "Track"
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    enabled: objectInspector.hasInspector
                        && inspectorData.targetKind !== undefined
                        && inspectorData.targetId !== undefined
                    onClicked: {
                        if (overlayRoot.inspectorIsTracked) {
                            skyContext.clearTrackedTarget()
                            return
                        }

                        skyContext.trackSearchTarget(inspectorData.targetKind, inspectorData.targetId)
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: overlayRoot.inspectorIsTracked
                        ? "Stop tracking object"
                        : "Track object and keep it centered"

                    contentItem: Text {
                        text: trackInspectorButton.text
                        color: trackInspectorButton.enabled
                            ? theme.toolbarButtonText
                            : theme.toolbarSecondaryText
                        font: trackInspectorButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: 6
                        color: trackInspectorButton.down
                            ? theme.toolbarButtonBackgroundPressed
                            : (trackInspectorButton.hovered
                                   ? theme.toolbarButtonBackgroundHover
                                   : theme.toolbarButtonBackground)
                        border.width: 1
                        border.color: overlayRoot.inspectorIsTracked
                            ? theme.selectionMarkerBorder
                            : theme.toolbarButtonBorder
                    }
                }

                ToolButton {
                    id: pinInspectorButton
                    objectName: objectInspector.inspectorPinned
                        ? "objectInspectorUnpinButton"
                        : "objectInspectorPinButton"
                    width: 52
                    height: 22
                    text: objectInspector.inspectorPinned ? "Unpin" : "Pin"
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    enabled: objectInspector.hasInspector
                    onClicked: {
                        if (objectInspector.inspectorPinned) {
                            sceneModel.setSelectedObjectInspectorPinned(false)
                            return
                        }

                        sceneModel.moveSelectedObjectInspector(objectInspector.x, objectInspector.y)
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: objectInspector.inspectorPinned
                        ? "Let inspector follow object"
                        : "Pin inspector on screen"

                    contentItem: Text {
                        text: pinInspectorButton.text
                        color: pinInspectorButton.enabled
                            ? theme.toolbarButtonText
                            : theme.toolbarSecondaryText
                        font: pinInspectorButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: 6
                        color: pinInspectorButton.down
                            ? theme.toolbarButtonBackgroundPressed
                            : (pinInspectorButton.hovered
                                   ? theme.toolbarButtonBackgroundHover
                                   : theme.toolbarButtonBackground)
                        border.width: 1
                        border.color: objectInspector.inspectorPinned
                            ? theme.selectionMarkerBorder
                            : theme.toolbarButtonBorder
                    }
                }

                ToolButton {
                    id: closeInspectorButton
                    objectName: "objectInspectorCloseButton"
                    width: 22
                    height: 22
                    text: "x"
                    font.family: "Avenir Next"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    onClicked: {
                        sceneModel.clearSelectedObjectInspector()
                        skyContext.clearSelectedSearchTarget()
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Close inspector"

                    contentItem: Text {
                        text: closeInspectorButton.text
                        color: theme.toolbarButtonText
                        font: closeInspectorButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: closeInspectorButton.down
                            ? theme.toolbarButtonBackgroundPressed
                            : (closeInspectorButton.hovered
                                   ? theme.toolbarButtonBackgroundHover
                                   : theme.toolbarButtonBackground)
                        border.width: 1
                        border.color: theme.toolbarButtonBorder
                    }
                }
            }

            Repeater {
                model: objectInspector.hasInspector && inspectorData.fields
                    ? inspectorData.fields
                    : []

                delegate: Row {
                    required property var modelData
                    width: inspectorContent.width
                    height: Math.max(fieldLabel.implicitHeight, fieldValue.implicitHeight)
                    spacing: 8

                    Text {
                        id: fieldLabel
                        width: 92
                        text: modelData.label
                        color: theme.toolbarSecondaryText
                        font.family: "Avenir Next"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }

                    Text {
                        id: fieldValue
                        width: parent.width - 100
                        text: modelData.value
                        color: theme.toolbarPrimaryText
                        font.family: "Avenir Next"
                        font.pixelSize: 10
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }

            Row {
                visible: objectInspector.hasInspector
                    && inspectorData.aliases !== undefined
                    && inspectorData.aliases.length > 0
                width: parent.width
                spacing: 8

                Text {
                    width: 92
                    text: "Aliases"
                    color: theme.toolbarSecondaryText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width - 100
                    text: objectInspector.hasInspector && inspectorData.aliases !== undefined
                        ? inspectorData.aliases
                        : ""
                    color: theme.toolbarPrimaryText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
        }
    }

    Repeater {
        model: sceneModel.overlayItems

        delegate: Item {
            id: overlayDelegateRoot
            objectName: (modelData.kind === "cardinal" ? "cardinalOverlayLabel_" : "skyOverlayLabel_")
                + modelData.text
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
