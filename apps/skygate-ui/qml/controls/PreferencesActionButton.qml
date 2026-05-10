import QtQuick
import QtQuick.Controls

Rectangle {
    id: actionButton
    readonly property var theme: skyContext.theme
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
                   ? actionButton.theme.actionButtonDisabledTop
                   : (actionButton.primary
                          ? (actionMouse.pressed
                                 ? actionButton.theme.actionButtonPrimaryTopPressed
                                 : (actionMouse.containsMouse
                                        ? actionButton.theme.actionButtonPrimaryTopHover
                                        : actionButton.theme.actionButtonPrimaryTop))
                          : (actionMouse.pressed
                                 ? actionButton.theme.actionButtonSecondaryTopPressed
                                 : (actionMouse.containsMouse
                                        ? actionButton.theme.actionButtonSecondaryTopHover
                                        : actionButton.theme.actionButtonSecondaryTop)))
        }
        GradientStop {
            position: 1.0
            color: !actionButton.enabled
                   ? actionButton.theme.actionButtonDisabledBottom
                   : (actionButton.primary
                          ? (actionMouse.pressed
                                 ? actionButton.theme.actionButtonPrimaryBottomPressed
                                 : (actionMouse.containsMouse
                                        ? actionButton.theme.actionButtonPrimaryBottomHover
                                        : actionButton.theme.actionButtonPrimaryBottom))
                          : (actionMouse.pressed
                                 ? actionButton.theme.actionButtonSecondaryBottomPressed
                                 : (actionMouse.containsMouse
                                        ? actionButton.theme.actionButtonSecondaryBottomHover
                                        : actionButton.theme.actionButtonSecondaryBottom)))
        }
    }
    border.width: 1
    border.color: !enabled
        ? actionButton.theme.actionButtonDisabledBorder
        : (primary
               ? actionButton.theme.actionButtonPrimaryBorder
               : actionButton.theme.actionButtonSecondaryBorder)
    opacity: enabled ? 1.0 : 0.72

    Label {
        anchors.centerIn: parent
        text: actionButton.text
        color: actionButton.enabled
            ? actionButton.theme.actionButtonText
            : actionButton.theme.actionButtonTextDisabled
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
