import QtQuick
import QtQuick.Controls
pragma ComponentBehavior: Bound

Rectangle {
    id: objectInspector
    objectName: "objectInspector"
    required property var theme
    required property var skyContextController
    required property var sceneModel
    required property var inspectorData
    required property bool inspectorIsTracked
    required property Item overlayLayer
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
            Math.max(8, overlayLayer.width - objectInspector.width - 8)
        )
    }

    function clampedInspectorY(value) {
        return Math.min(
            Math.max(8, value),
            Math.max(8, overlayLayer.height - objectInspector.height - 8)
        )
    }

    function pointerPositionInOverlay(mouse) {
        return objectInspector.mapToItem(overlayLayer, mouse.x, mouse.y)
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
            objectInspector.sceneModel.moveSelectedObjectInspector(nextX, nextY)
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
                text: objectInspector.hasInspector ? objectInspector.inspectorData.title : ""
                color: objectInspector.theme.toolbarPrimaryText
                font.family: "Avenir Next"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                height: parent.height
            }

            SkyObjectInspectorButton {
                id: centerInspectorButton
                objectName: "objectInspectorCenterButton"
                theme: objectInspector.theme
                width: 64
                height: 22
                text: "Center"
                enabled: objectInspector.hasInspector
                    && objectInspector.inspectorData.targetKind !== undefined
                    && objectInspector.inspectorData.targetId !== undefined
                onClicked: {
                    objectInspector.skyContextController.focusSearchTarget(
                        objectInspector.inspectorData.targetKind,
                        objectInspector.inspectorData.targetId
                    )
                }
                ToolTip.text: "Center view on object"
            }

            SkyObjectInspectorButton {
                id: trackInspectorButton
                objectName: objectInspector.inspectorIsTracked
                    ? "objectInspectorUntrackButton"
                    : "objectInspectorTrackButton"
                theme: objectInspector.theme
                borderColor: objectInspector.inspectorIsTracked
                    ? objectInspector.theme.selectionMarkerBorder
                    : objectInspector.theme.toolbarButtonBorder
                width: 64
                height: 22
                text: objectInspector.inspectorIsTracked ? "Untrack" : "Track"
                enabled: objectInspector.hasInspector
                    && objectInspector.inspectorData.targetKind !== undefined
                    && objectInspector.inspectorData.targetId !== undefined
                onClicked: {
                    if (objectInspector.inspectorIsTracked) {
                        objectInspector.skyContextController.clearTrackedTarget()
                        return
                    }

                    objectInspector.skyContextController.trackSearchTarget(
                        objectInspector.inspectorData.targetKind,
                        objectInspector.inspectorData.targetId
                    )
                }
                ToolTip.text: objectInspector.inspectorIsTracked
                    ? "Stop tracking object"
                    : "Track object and keep it centered"
            }

            SkyObjectInspectorButton {
                id: pinInspectorButton
                objectName: objectInspector.inspectorPinned
                    ? "objectInspectorUnpinButton"
                    : "objectInspectorPinButton"
                theme: objectInspector.theme
                borderColor: objectInspector.inspectorPinned
                    ? objectInspector.theme.selectionMarkerBorder
                    : objectInspector.theme.toolbarButtonBorder
                width: 52
                height: 22
                text: objectInspector.inspectorPinned ? "Unpin" : "Pin"
                enabled: objectInspector.hasInspector
                onClicked: {
                    if (objectInspector.inspectorPinned) {
                        objectInspector.sceneModel.setSelectedObjectInspectorPinned(false)
                        return
                    }

                    objectInspector.sceneModel.moveSelectedObjectInspector(
                        objectInspector.x,
                        objectInspector.y
                    )
                }
                ToolTip.text: objectInspector.inspectorPinned
                    ? "Let inspector follow object"
                    : "Pin inspector on screen"
            }

            SkyObjectInspectorButton {
                id: closeInspectorButton
                objectName: "objectInspectorCloseButton"
                theme: objectInspector.theme
                width: 22
                height: 22
                text: "x"
                font.pixelSize: 12
                onClicked: {
                    objectInspector.sceneModel.clearSelectedObjectInspector()
                    objectInspector.skyContextController.clearSelectedSearchTarget()
                }
                ToolTip.text: "Close inspector"
            }
        }

        Repeater {
            model: objectInspector.hasInspector && objectInspector.inspectorData.fields
                ? objectInspector.inspectorData.fields
                : []

            delegate: Row {
                id: inspectorFieldRow
                required property var modelData
                width: inspectorContent.width
                height: Math.max(fieldLabel.implicitHeight, fieldValue.implicitHeight)
                spacing: 8

                Text {
                    id: fieldLabel
                    width: 92
                    text: inspectorFieldRow.modelData.label
                    color: objectInspector.theme.toolbarSecondaryText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Text {
                    id: fieldValue
                    width: parent.width - 100
                    text: inspectorFieldRow.modelData.value
                    color: objectInspector.theme.toolbarPrimaryText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }

        Row {
            visible: objectInspector.hasInspector
                && objectInspector.inspectorData.aliases !== undefined
                && objectInspector.inspectorData.aliases.length > 0
            width: parent.width
            spacing: 8

            Text {
                width: 92
                text: "Aliases"
                color: objectInspector.theme.toolbarSecondaryText
                font.family: "Avenir Next"
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            Text {
                width: parent.width - 100
                text: objectInspector.hasInspector
                    && objectInspector.inspectorData.aliases !== undefined
                    ? objectInspector.inspectorData.aliases
                    : ""
                color: objectInspector.theme.toolbarPrimaryText
                font.family: "Avenir Next"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }
}
