import QtQuick
import QtQuick.Controls

Rectangle {
    id: actionButton
    readonly property var theme: skyContext.theme
    property string text: ""
    signal clicked()

    function click() {
        clicked()
    }

    implicitWidth: label.implicitWidth + 16
    implicitHeight: 20
    radius: 4
    color: !enabled
        ? actionButton.theme.actionButtonDisabledTop
        : (mouse.pressed
               ? actionButton.theme.actionButtonSecondaryTopPressed
               : (mouse.containsMouse
                      ? actionButton.theme.actionButtonSecondaryTopHover
                      : actionButton.theme.actionButtonSecondaryTop))
    border.width: 1
    border.color: enabled
        ? actionButton.theme.actionButtonSecondaryBorder
        : actionButton.theme.actionButtonDisabledBorder
    opacity: enabled ? 1.0 : 0.62

    Label {
        id: label
        anchors.centerIn: parent
        text: actionButton.text
        color: actionButton.enabled
            ? actionButton.theme.actionButtonText
            : actionButton.theme.actionButtonTextDisabled
        font.family: "Avenir Next"
        font.pixelSize: 9
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: actionButton.enabled
        enabled: actionButton.enabled
        cursorShape: actionButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: actionButton.clicked()
    }
}
