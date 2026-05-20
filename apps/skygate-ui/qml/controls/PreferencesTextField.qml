import QtQuick
import QtQuick.Controls

TextField {
    id: textControl
    readonly property var theme: skyContext.theme
    property string suffix: ""
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
    rightPadding: suffixLabel.visible ? suffixLabel.width + 18 : 9
    topPadding: 5
    bottomPadding: 5

    background: Rectangle {
        radius: 8
        color: textControl.enabled ? theme.inputBackground : theme.inputBackgroundDisabled
        border.width: 1
        border.color: textControl.activeFocus ? theme.inputBorderFocus : theme.inputBorder
    }

    Label {
        id: suffixLabel
        visible: textControl.suffix.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        text: textControl.suffix
        color: textControl.enabled ? theme.inputPlaceholderText : theme.actionButtonTextDisabled
        font.family: textControl.font.family
        font.pixelSize: 10
    }
}
