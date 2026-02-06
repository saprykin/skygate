import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    width: 1100
    height: 760
    visible: true
    title: "Skygate"

    header: ToolBar {
        Row {
            id: toolbarRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            padding: 8

            Button {
                id: liveButton
                text: "Live"
                anchors.verticalCenter: parent.verticalCenter
            }
            Button {
                text: "Pause"
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                id: utcTimeLabel
                text: "UTC Time"
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: utcTimeInput
                width: 220
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "2026-02-06T22:00:00Z"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#060c1a"

        Text {
            anchors.centerIn: parent
            text: "Sky rendering placeholder"
            color: "#d7e3ff"
            font.pixelSize: 22
        }
    }
}
