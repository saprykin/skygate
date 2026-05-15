import QtQuick

MouseArea {
    id: interactionLayer
    objectName: "skyInteractionLayer"
    required property Item viewportItem
    required property var skyContextController
    required property var skySceneModel

    anchors.fill: viewportItem
    hoverEnabled: true
    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

    property real lastX: 0
    property real lastY: 0
    property real pressX: 0
    property real pressY: 0
    property real hoverX: 0
    property real hoverY: 0
    property string hoveredObjectLabel: ""
    property bool draggedSincePress: false
    property real pendingPanDeltaX: 0
    property real pendingPanDeltaY: 0
    property int pendingZoomWheelDeltaY: 0
    property real pendingZoomScaleDelta: 1
    property real pendingZoomHoverX: 0
    property real pendingZoomHoverY: 0
    property bool pendingZoomHoverUpdate: false
    readonly property real azimuthSensitivity: 0.18
    readonly property real altitudeSensitivity: 0.18
    readonly property real dragThresholdPx: 5
    readonly property int panUpdateIntervalMs: 16
    readonly property int zoomUpdateIntervalMs: 16

    function flushPendingPan() {
        if (pendingPanDeltaX === 0 && pendingPanDeltaY === 0) {
            return
        }

        const deltaX = pendingPanDeltaX
        const deltaY = pendingPanDeltaY
        pendingPanDeltaX = 0
        pendingPanDeltaY = 0
        skyContextController.panViewBy(
            -deltaX * azimuthSensitivity,
            -deltaY * altitudeSensitivity
        )
    }

    function queuePan(deltaX, deltaY) {
        pendingPanDeltaX += deltaX
        pendingPanDeltaY += deltaY
        if (!panTimer.running) {
            panTimer.start()
        }
    }

    function flushPendingZoom() {
        if (pendingZoomWheelDeltaY !== 0) {
            const wheelDeltaY = pendingZoomWheelDeltaY
            pendingZoomWheelDeltaY = 0
            skyContextController.zoomViewByWheelDelta(wheelDeltaY)
        }

        if (pendingZoomScaleDelta !== 1) {
            const scaleDelta = pendingZoomScaleDelta
            pendingZoomScaleDelta = 1
            skyContextController.zoomViewByScaleDelta(scaleDelta)
        }

        if (pendingZoomHoverUpdate) {
            pendingZoomHoverUpdate = false
            hoveredObjectLabel = skySceneModel.objectLabelAt(pendingZoomHoverX, pendingZoomHoverY)
        }
    }

    function queueWheelZoom(deltaY, x, y) {
        pendingZoomWheelDeltaY += deltaY
        pendingZoomHoverX = x
        pendingZoomHoverY = y
        pendingZoomHoverUpdate = true
        if (!zoomTimer.running) {
            zoomTimer.start()
        }
    }

    function queueScaleZoom(delta) {
        pendingZoomScaleDelta *= delta
        pendingZoomHoverUpdate = false
        hoveredObjectLabel = ""
        if (!zoomTimer.running) {
            zoomTimer.start()
        }
    }

    Timer {
        id: panTimer
        interval: interactionLayer.panUpdateIntervalMs
        repeat: false

        onTriggered: interactionLayer.flushPendingPan()
    }

    Timer {
        id: zoomTimer
        interval: interactionLayer.zoomUpdateIntervalMs
        repeat: false

        onTriggered: interactionLayer.flushPendingZoom()
    }

    onPressed: function(mouse) {
        lastX = mouse.x
        lastY = mouse.y
        pressX = mouse.x
        pressY = mouse.y
        pendingPanDeltaX = 0
        pendingPanDeltaY = 0
        panTimer.stop()
        draggedSincePress = false
    }

    onPositionChanged: function(mouse) {
        hoverX = mouse.x
        hoverY = mouse.y

        if (!(mouse.buttons & Qt.LeftButton)) {
            hoveredObjectLabel = skySceneModel.objectLabelAt(mouse.x, mouse.y)
            return
        }

        const pressDeltaX = mouse.x - pressX
        const pressDeltaY = mouse.y - pressY
        if (!draggedSincePress
                && ((pressDeltaX * pressDeltaX) + (pressDeltaY * pressDeltaY))
                    < (dragThresholdPx * dragThresholdPx)) {
            return
        }

        draggedSincePress = true
        const deltaX = mouse.x - lastX
        const deltaY = mouse.y - lastY
        queuePan(deltaX, deltaY)
        lastX = mouse.x
        lastY = mouse.y
        hoveredObjectLabel = ""
    }

    onReleased: function(mouse) {
        if (draggedSincePress) {
            flushPendingPan()
            panTimer.stop()
        }

        if (mouse.button !== Qt.LeftButton || draggedSincePress) {
            return
        }

        hoverX = mouse.x
        hoverY = mouse.y
        if (!skySceneModel.selectObjectAt(mouse.x, mouse.y)) {
            skyContextController.clearSelectedSearchTarget()
        }
        hoveredObjectLabel = skySceneModel.objectLabelAt(mouse.x, mouse.y)
    }

    onWheel: function(wheel) {
        queueWheelZoom(wheel.angleDelta.y, wheel.x, wheel.y)
        wheel.accepted = true
    }

    PinchHandler {
        objectName: "skyInteractionPinchHandler"
        target: null

        onScaleChanged: function(delta) {
            interactionLayer.queueScaleZoom(delta)
        }
    }

    onExited: hoveredObjectLabel = ""

    onCanceled: {
        flushPendingPan()
        flushPendingZoom()
        panTimer.stop()
        zoomTimer.stop()
        hoveredObjectLabel = ""
    }
}
