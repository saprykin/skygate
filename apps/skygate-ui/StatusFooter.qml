import QtQuick
import QtQuick.Controls

Rectangle {
    id: footerRoot
    required property var skyContextController
    property bool dateTimePopupOpen: false
    signal dateTimeClicked()

    height: 48
    color: "#0b1428"

    Row {
        id: statusLeftRow
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: statusTimeArea.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 20

        Label {
            text: footerRoot.skyContextController.live ? "Live: On" : "Live: Off"
            color: footerRoot.skyContextController.live ? "#7fe39f" : "#f2b0b0"
        }
        Label {
            text: footerRoot.skyContextController.locationStatusText
            color: "#a9c4ff"
        }
        Label {
            text: "Lat " + footerRoot.skyContextController.latitudeText
                  + " | Lon " + footerRoot.skyContextController.longitudeText
                  + " | Elev " + footerRoot.skyContextController.elevationText + " m"
                  + " | Proj " + footerRoot.skyContextController.projectionTypeText
                  + " | View Alt "
                  + Number(footerRoot.skyContextController.viewCenterAltitudeDeg).toFixed(1)
                  + " Az "
                  + Number(footerRoot.skyContextController.viewCenterAzimuthDeg).toFixed(1)
                  + " | Mag ≤ "
                  + Number(footerRoot.skyContextController.magnitudeCutoff).toFixed(1)
            color: "#9ab0d6"
            elide: Text.ElideRight
            width: Math.max(120, statusLeftRow.width - 320)
        }
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
            color: "#d7e3ff"
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
            color: statusTimeMouse.containsMouse ? "#10203d" : "transparent"
            border.width: 1
            border.color: (statusTimeMouse.containsMouse || footerRoot.dateTimePopupOpen)
                ? "#3a5885"
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
