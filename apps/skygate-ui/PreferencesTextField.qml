import QtQuick
import QtQuick.Controls

TextField {
    id: textControl
    font.family: "Avenir Next"
    font.pixelSize: 11
    implicitHeight: 32
    color: "#f1f3ff"
    horizontalAlignment: Text.AlignLeft
    placeholderTextColor: "#8187ab"
    selectedTextColor: "#f4f6ff"
    selectionColor: "#4068b4"
    leftPadding: 9
    rightPadding: 9
    topPadding: 5
    bottomPadding: 5

    background: Rectangle {
        radius: 8
        color: textControl.enabled ? "#232742" : "#1b1e33"
        border.width: 1
        border.color: textControl.activeFocus ? "#628fd9" : "#4d5378"
    }
}
