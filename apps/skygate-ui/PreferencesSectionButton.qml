import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: ""
    property bool active: false
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: 38
    radius: 10
    color: active
           ? "#2f79b8"
           : (buttonMouse.pressed ? "#213c5b" : (buttonMouse.containsMouse ? "#1c3452" : "#142943"))
    border.width: 1
    border.color: active ? "#9de2ff" : "#4f769e"

    Label {
        anchors.centerIn: parent
        text: root.label
        color: root.active ? "#ecf8ff" : "#bdd4f4"
        font.family: "Avenir Next"
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: buttonMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
