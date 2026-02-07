import QtQuick
import QtQuick.Controls

CheckBox {
    id: checkControl
    implicitWidth: 24
    implicitHeight: 24
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    indicator: Rectangle {
        implicitWidth: 24
        implicitHeight: 24
        radius: 5
        color: checkControl.checked ? "#2f79b8" : "#102544"
        border.width: 1
        border.color: checkControl.checked ? "#9de2ff" : "#5c83aa"

        Text {
            anchors.centerIn: parent
            text: "\u2713"
            color: "#ecf8ff"
            font.pixelSize: 16
            font.weight: Font.DemiBold
            visible: checkControl.checked
        }
    }

    contentItem: Item {}
}
