import QtQuick
import QtQuick.Controls

ToolButton {
    id: inspectorButton
    required property var theme
    property color borderColor: theme.toolbarButtonBorder

    font.family: "Avenir Next"
    font.pixelSize: 11
    font.weight: Font.DemiBold
    ToolTip.visible: hovered
    ToolTip.delay: 250

    contentItem: Text {
        text: inspectorButton.text
        color: inspectorButton.enabled
            ? inspectorButton.theme.toolbarButtonText
            : inspectorButton.theme.toolbarSecondaryText
        font: inspectorButton.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 6
        color: inspectorButton.down
            ? inspectorButton.theme.toolbarButtonBackgroundPressed
            : (inspectorButton.hovered
                   ? inspectorButton.theme.toolbarButtonBackgroundHover
                   : inspectorButton.theme.toolbarButtonBackground)
        border.width: 1
        border.color: inspectorButton.borderColor
    }
}
