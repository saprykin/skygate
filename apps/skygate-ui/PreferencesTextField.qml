import QtQuick
import QtQuick.Controls

TextField {
    id: textControl
    font.family: "Avenir Next"
    implicitHeight: 36
    color: "#ecf4ff"
    horizontalAlignment: Text.AlignLeft
    placeholderTextColor: "#86a7cf"
    selectedTextColor: "#f4fbff"
    selectionColor: "#4f90cd"
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        radius: 9
        color: "#102544"
        border.width: 1
        border.color: textControl.activeFocus ? "#8fd9ff" : "#3f648d"
    }
}
