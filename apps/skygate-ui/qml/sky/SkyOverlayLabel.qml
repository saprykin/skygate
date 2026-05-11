import QtQuick
import QtQuick.Controls

Item {
    id: overlayDelegateRoot
    objectName: (modelData.kind === "cardinal" ? "cardinalOverlayLabel_" : "skyOverlayLabel_")
        + modelData.text
    required property var modelData
    required property var theme
    required property var avoidance
    readonly property bool isCardinal: modelData.kind === "cardinal"
    readonly property color labelColor: modelData.color
        ? modelData.color
        : theme.overlayLabelText
    readonly property real labelWidth: overlayLabel.implicitWidth + 12
    readonly property real labelHeight: overlayLabel.implicitHeight + 6
    readonly property real labelX: modelData.x - (labelWidth * 0.5)
    readonly property real labelPreferredY: modelData.y - labelHeight - 8

    x: labelX
    y: isCardinal
            ? avoidance.adjustedYToAvoidItems(
                labelX,
                labelPreferredY,
                labelWidth,
                labelHeight
            )
            : labelPreferredY
    width: labelWidth
    height: labelHeight
    visible: isCardinal
            ? true
            : !avoidance.overlapsAvoidItems(x, y, width, height)

    Rectangle {
        x: 0
        y: 0
        width: overlayDelegateRoot.labelWidth
        height: overlayDelegateRoot.labelHeight
        radius: overlayDelegateRoot.isCardinal ? 6 : 5
        color: overlayDelegateRoot.isCardinal
            ? overlayDelegateRoot.theme.overlayCardinalLabelBackground
            : overlayDelegateRoot.theme.overlayLabelBackground
        border.width: 1
        border.color: overlayDelegateRoot.labelColor
        z: overlayDelegateRoot.isCardinal ? 9 : 8

        Label {
            id: overlayLabel
            anchors.centerIn: parent
            text: overlayDelegateRoot.modelData.text
            color: overlayDelegateRoot.labelColor
            font.family: "Avenir Next"
            font.pixelSize: overlayDelegateRoot.isCardinal ? 14 : 11
            font.bold: true
        }
    }
}
