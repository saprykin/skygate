import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: ""
    property bool active: false
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: 30
    radius: 8
    gradient: Gradient {
        GradientStop {
            position: 0.0
            color: root.active
                   ? (buttonMouse.pressed ? "#355f9f"
                                          : (buttonMouse.containsMouse ? "#3e70b8" : "#3867a9"))
                   : (buttonMouse.pressed ? "#20233a"
                                          : (buttonMouse.containsMouse ? "#242943" : "#20243d"))
        }
        GradientStop {
            position: 1.0
            color: root.active
                   ? (buttonMouse.pressed ? "#2c548f"
                                          : (buttonMouse.containsMouse ? "#365f9d" : "#30598f"))
                   : (buttonMouse.pressed ? "#1a1d31"
                                          : (buttonMouse.containsMouse ? "#1f2237" : "#1b1f33"))
        }
    }
    border.width: 1
    border.color: root.active ? "#5d91dc" : "#242843"

    Label {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 11
        text: root.label
        color: root.active ? "#f3f5ff" : "#eef0fd"
        font.family: "Avenir Next"
        font.pixelSize: 11
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
