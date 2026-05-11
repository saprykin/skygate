import QtQuick
import QtQuick.Controls

Rectangle {
    id: hoverObjectLabel
    objectName: "hoverObjectLabel"
    required property var theme
    required property var interactionLayer

    visible: interactionLayer.hoveredObjectLabel.length > 0
    x: Math.min(interactionLayer.hoverX + 14, Math.max(0, parent.width - width - 8))
    y: Math.min(interactionLayer.hoverY + 14, Math.max(0, parent.height - height - 8))
    radius: 6
    color: theme.overlayTooltipBackground
    border.width: 1
    border.color: theme.overlayTooltipBorder
    z: 10
    width: hoverLabelText.implicitWidth + 14
    height: hoverLabelText.implicitHeight + 8

    Label {
        id: hoverLabelText
        anchors.centerIn: parent
        text: hoverObjectLabel.interactionLayer.hoveredObjectLabel
        color: hoverObjectLabel.theme.overlayTooltipText
        font.family: "Avenir Next"
        font.pixelSize: 14
    }
}
