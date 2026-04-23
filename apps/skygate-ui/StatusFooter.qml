import QtQuick
import QtQuick.Controls

Rectangle {
    id: footerRoot
    readonly property var theme: skyContextController.theme
    required property var skyContextController
    property bool dateTimePopupOpen: false
    signal dateTimeClicked()

    height: 48
    color: theme.footerBackground

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
