import QtQuick
import QtQuick.Controls

Rectangle {
    id: actionButton
    property string text: ""
    signal clicked()

    implicitWidth: 160
    implicitHeight: 36
    radius: 9
    color: !enabled
           ? "#1a2b43"
           : (actionMouse.pressed ? "#2a4e78" : (actionMouse.containsMouse ? "#356392" : "#2f5a87"))
    border.width: 1
    border.color: enabled ? "#8dcfff" : "#4d6d92"
    opacity: enabled ? 1.0 : 0.65

    Label {
        anchors.centerIn: parent
        text: actionButton.text
        color: "#edf8ff"
        font.family: "Avenir Next"
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: actionMouse
        anchors.fill: parent
        hoverEnabled: actionButton.enabled
        enabled: actionButton.enabled
        cursorShape: actionButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: actionButton.clicked()
    }
}
