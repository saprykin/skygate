import QtQuick
import QtQuick.Controls

CheckBox {
    id: checkControl
    implicitWidth: 18
    implicitHeight: 18
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        radius: 4
        color: "#232742"
        border.width: 1
        border.color: checkControl.checked ? "#628fd9" : "#52587d"

        Text {
            anchors.centerIn: parent
            text: "\u2713"
            color: "#d7e6ff"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            visible: checkControl.checked
        }
    }

    contentItem: Item {}
}
