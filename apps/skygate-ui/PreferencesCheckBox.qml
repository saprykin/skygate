import QtQuick
import QtQuick.Controls

CheckBox {
    id: checkControl
    readonly property var theme: skyContext.theme
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
        color: checkControl.theme.checkBackground
        border.width: 1
        border.color: checkControl.checked
            ? checkControl.theme.checkBorderChecked
            : checkControl.theme.checkBorder

        Text {
            anchors.centerIn: parent
            text: "\u2713"
            color: checkControl.theme.checkMarkColor
            font.pixelSize: 10
            font.weight: Font.DemiBold
            visible: checkControl.checked
        }
    }

    contentItem: Item {}
}
