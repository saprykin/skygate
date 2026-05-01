import QtQuick
import QtQuick.Controls

Rectangle {
    id: footerRoot
    readonly property var theme: skyContextController.theme
    required property var skyContextController
    required property var sceneModel
    property bool dateTimePopupOpen: false
    signal dateTimeClicked()

    function trackedInspectorVisible() {
        const inspector = sceneModel.selectedObjectInspector
        return inspector
            && inspector.visible === true
            && inspector.targetKind === skyContextController.trackedTargetKind
            && inspector.targetId === skyContextController.trackedTargetId
    }

    height: 48
    color: theme.footerBackground

    Row {
        id: statusLeftRow
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: trackingIndicator.visible ? trackingIndicator.left : statusTimeArea.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 20

        Label {
            text: footerRoot.skyContextController.live ? "Live: On" : "Live: Off"
            color: footerRoot.skyContextController.live ? theme.footerLiveText : theme.footerPausedText
        }
        Label {
            text: footerRoot.skyContextController.locationStatusText
            color: theme.footerLocationText
        }
        Label {
            text: "Lat " + footerRoot.skyContextController.latitudeText
                  + " | Lon " + footerRoot.skyContextController.longitudeText
                  + " | Elev " + footerRoot.skyContextController.elevationText + " m"
                  + " | View Alt "
                  + Number(footerRoot.skyContextController.viewCenterAltitudeDeg).toFixed(1)
                  + " Az "
                  + Number(footerRoot.skyContextController.viewCenterAzimuthDeg).toFixed(1)
            color: theme.footerInfoText
            elide: Text.ElideRight
            width: Math.max(120, statusLeftRow.width - 320)
        }
    }

    Item {
        id: trackingIndicator
        visible: footerRoot.skyContextController.hasTrackedTarget
        x: statusTimeArea.x + statusTimeHighlight.x - width - 2
        anchors.verticalCenter: parent.verticalCenter
        width: visible ? 32 : 0
        height: footerRoot.height

        Rectangle {
            id: trackingHighlight
            anchors.centerIn: parent
            width: 26
            height: 26
            radius: 8
            color: trackingMouse.containsMouse ? theme.footerTimeHoverBackground : "transparent"
            border.width: 1
            border.color: trackingMouse.containsMouse
                ? theme.footerTimeHoverBorder
                : "transparent"
        }

        Canvas {
            id: trackingIcon
            anchors.centerIn: trackingHighlight
            width: 22
            height: 22
            antialiasing: true

            property color bodyColor: theme.footerTimeText

            onBodyColorChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                const cx = 6.6
                const cy = 7.0
                const radius = 4.4
                ctx.reset()
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.strokeStyle = bodyColor
                ctx.fillStyle = bodyColor

                ctx.lineWidth = 1.2
                ctx.beginPath()
                ctx.moveTo(cx + 2.9, cy - 3.0)
                ctx.lineTo(21.0, 5.8)
                ctx.moveTo(cx + 2.5, cy + 1.0)
                ctx.lineTo(20.4, 14.5)
                ctx.moveTo(cx + 1.3, cy + 2.7)
                ctx.lineTo(16.6, 17.6)
                ctx.moveTo(cx - 2.0, cy + 3.8)
                ctx.lineTo(13.0, 21.0)
                ctx.stroke()

                ctx.beginPath()
                ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                ctx.fill()
            }
        }

        MouseArea {
            id: trackingMouse
            anchors.fill: trackingHighlight
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (footerRoot.trackedInspectorVisible()) {
                    footerRoot.sceneModel.clearSelectedObjectInspector()
                    footerRoot.skyContextController.clearSelectedSearchTarget()
                    return
                }

                footerRoot.skyContextController.focusSearchTarget(
                    footerRoot.skyContextController.trackedTargetKind,
                    footerRoot.skyContextController.trackedTargetId
                )
            }
        }

        ToolTip.visible: trackingMouse.containsMouse
        ToolTip.delay: 250
        ToolTip.text: "Tracking " + footerRoot.skyContextController.trackedTargetDisplayText
    }

    Item {
        id: statusTimeArea
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 280
        height: footerRoot.height

        Label {
            id: statusTimeLabel
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignRight
            text: footerRoot.skyContextController.utcDateText
                  + " "
                  + footerRoot.skyContextController.utcTimeText
                  + " UTC"
            color: theme.footerTimeText
            font.family: "Menlo"
            elide: Text.ElideLeft
            z: 1
        }

        Rectangle {
            id: statusTimeHighlight
            anchors.right: parent.right
            anchors.rightMargin: -10
            anchors.verticalCenter: parent.verticalCenter
            width: statusTimeLabel.implicitWidth + 20
            height: statusTimeLabel.implicitHeight + 8
            radius: 8
            color: statusTimeMouse.containsMouse ? theme.footerTimeHoverBackground : "transparent"
            border.width: 1
            border.color: (statusTimeMouse.containsMouse || footerRoot.dateTimePopupOpen)
                ? theme.footerTimeHoverBorder
                : "transparent"
            z: 0
        }

        MouseArea {
            id: statusTimeMouse
            anchors.fill: statusTimeHighlight
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: footerRoot.dateTimeClicked()
            z: 2
        }
    }
}
