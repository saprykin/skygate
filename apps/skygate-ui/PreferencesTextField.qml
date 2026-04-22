import QtQuick
import QtQuick.Controls

TextField {
    id: textControl
    readonly property var theme: skyContext.theme
    font.family: "Avenir Next"
    font.pixelSize: 11
    implicitHeight: 32
    color: theme.inputText
    selectByMouse: true
    horizontalAlignment: Text.AlignLeft
    placeholderTextColor: theme.inputPlaceholderText
    selectedTextColor: theme.inputSelectionText
    selectionColor: theme.inputSelection
    leftPadding: 9
    rightPadding: 9
    topPadding: 5
    bottomPadding: 5

    background: Rectangle {
        radius: 8
        color: textControl.enabled ? theme.inputBackground : theme.inputBackgroundDisabled
        border.width: 1
        border.color: textControl.activeFocus ? theme.inputBorderFocus : theme.inputBorder
    }
}
