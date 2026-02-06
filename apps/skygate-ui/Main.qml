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
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            padding: 8

            Button { text: "Live" }
            Button { text: "Pause" }
            Label { text: "UTC Time" }
            TextField {
                width: 220
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
