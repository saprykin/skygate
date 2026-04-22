import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    readonly property var theme: skyContext.theme
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
                   ? (buttonMouse.pressed
                          ? root.theme.sectionButtonActiveTopPressed
                          : (buttonMouse.containsMouse
                                 ? root.theme.sectionButtonActiveTopHover
                                 : root.theme.sectionButtonActiveTop))
                   : (buttonMouse.pressed
                          ? root.theme.sectionButtonInactiveTopPressed
                          : (buttonMouse.containsMouse
                                 ? root.theme.sectionButtonInactiveTopHover
                                 : root.theme.sectionButtonInactiveTop))
        }
        GradientStop {
            position: 1.0
            color: root.active
                   ? (buttonMouse.pressed
                          ? root.theme.sectionButtonActiveBottomPressed
                          : (buttonMouse.containsMouse
                                 ? root.theme.sectionButtonActiveBottomHover
                                 : root.theme.sectionButtonActiveBottom))
                   : (buttonMouse.pressed
                          ? root.theme.sectionButtonInactiveBottomPressed
                          : (buttonMouse.containsMouse
                                 ? root.theme.sectionButtonInactiveBottomHover
                                 : root.theme.sectionButtonInactiveBottom))
        }
    }
    border.width: 1
    border.color: root.active
        ? root.theme.sectionButtonActiveBorder
        : root.theme.sectionButtonInactiveBorder

    Label {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 11
        text: root.label
        color: root.active
            ? root.theme.sectionButtonTextActive
            : root.theme.sectionButtonTextInactive
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
