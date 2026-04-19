import QtQuick

MouseArea {
    id: interactionLayer
    required property Item viewportItem
    required property var skyContextController
    required property var skySceneModel

    anchors.fill: viewportItem
    hoverEnabled: true
    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

    property real lastX: 0
    property real lastY: 0
    property real hoverX: 0
    property real hoverY: 0
    property string hoveredObjectLabel: ""
    readonly property real azimuthSensitivity: 0.18
    readonly property real altitudeSensitivity: 0.18

    onPressed: function(mouse) {
        lastX = mouse.x
        lastY = mouse.y
    }

    onPositionChanged: function(mouse) {
        hoverX = mouse.x
        hoverY = mouse.y

        if (!(mouse.buttons & Qt.LeftButton)) {
            hoveredObjectLabel = skySceneModel.objectLabelAt(mouse.x, mouse.y)
            return
        }

        const deltaX = mouse.x - lastX
        const deltaY = mouse.y - lastY
        skyContextController.panViewBy(
            -deltaX * azimuthSensitivity,
            -deltaY * altitudeSensitivity
        )
        lastX = mouse.x
        lastY = mouse.y
        hoveredObjectLabel = ""
    }

    onWheel: function(wheel) {
        skyContextController.zoomViewByWheelDelta(wheel.angleDelta.y)
        hoveredObjectLabel = skySceneModel.objectLabelAt(wheel.x, wheel.y)
        wheel.accepted = true
    }

    PinchHandler {
        target: null

        onScaleChanged: function(delta) {
            interactionLayer.skyContextController.zoomViewByScaleDelta(delta)
            interactionLayer.hoveredObjectLabel = ""
        }
    }

    onExited: hoveredObjectLabel = ""
}
