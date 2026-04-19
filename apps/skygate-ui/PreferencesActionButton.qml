import QtQuick
import QtQuick.Controls

Rectangle {
    id: actionButton
    property string text: ""
    property bool primary: false
    signal clicked()

    implicitWidth: primary ? 116 : 128
    implicitHeight: 32
    radius: 8
    gradient: Gradient {
        GradientStop {
            position: 0.0
            color: !actionButton.enabled
                   ? "#252943"
                   : (actionButton.primary
                          ? (actionMouse.pressed ? "#2d62a8"
                                                 : (actionMouse.containsMouse ? "#3d79c3" : "#386cb4"))
                          : (actionMouse.pressed ? "#20243d"
                                                 : (actionMouse.containsMouse ? "#262b47" : "#232741")))
        }
        GradientStop {
            position: 1.0
            color: !actionButton.enabled
                   ? "#1d2035"
                   : (actionButton.primary
                          ? (actionMouse.pressed ? "#29599a"
                                                 : (actionMouse.containsMouse ? "#366cad" : "#305f9d"))
                          : (actionMouse.pressed ? "#1a1e33"
                                                 : (actionMouse.containsMouse ? "#21253d" : "#1d2137")))
        }
    }
    border.width: 1
    border.color: !enabled ? "#353955" : (primary ? "#5f97e5" : "#3e4465")
    opacity: enabled ? 1.0 : 0.72

    Label {
        anchors.centerIn: parent
        text: actionButton.text
        color: actionButton.enabled ? "#f2f4ff" : "#8f95b2"
        font.family: "Avenir Next"
        font.pixelSize: 10
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
