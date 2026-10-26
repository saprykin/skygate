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
    // Keep the label inside the overlay layer when the projected body sits at
    // the viewport edge. Without clamping the label rendered partially outside
    // the window and the main-window rendering bounds check failed at scene
    // times that placed a bright star near the 560x640 window edge.
    readonly property real layerWidth: parent ? parent.width : labelWidth
    readonly property real layerHeight: parent ? parent.height : labelHeight
    readonly property real labelX: Math.max(
        0,
        Math.min(
            modelData.x - (labelWidth * 0.5),
            layerWidth - labelWidth
        )
    )
    readonly property real labelPreferredY: modelData.y - labelHeight - 8
    readonly property real labelY: Math.max(
        0,
        Math.min(
            isCardinal
                ? avoidance.adjustedYToAvoidItems(
                    labelX,
                    labelPreferredY,
                    labelWidth,
                    labelHeight
                )
                : labelPreferredY,
            layerHeight - labelHeight
        )
    )

    x: labelX
    y: labelY
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
