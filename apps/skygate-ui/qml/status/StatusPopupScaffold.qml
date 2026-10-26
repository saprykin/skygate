import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: popupRoot
    required property var skyContextController
    readonly property var theme: skyContextController.theme
    property bool opened: false
    property real popupRightMargin: 8
    property real popupBottomMargin: 56
    property real scrimTopMargin: 0
    property real cardWidth: 0
    property real cardMaxHeight: 0
    property string cardTitle: ""
    default property alias content: popupLayout.data

    anchors.fill: parent
    visible: opened
    enabled: opened
    z: 1000

    signal opening()
    signal closing()

    function open() {
        opening()
        opened = true
        forceActiveFocus()
    }

    function cancel() {
        close()
    }

    function close() {
        opened = false
        closing()
    }

    Keys.onEscapePressed: close()

    data: [
        MouseArea {
            objectName: popupRoot.objectName + "Scrim"
            anchors.fill: parent
            anchors.topMargin: popupRoot.scrimTopMargin
            onClicked: popupRoot.close()
        },
        Rectangle {
            id: popupCard
            objectName: popupRoot.objectName + "Card"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: popupRoot.popupRightMargin
            anchors.bottomMargin: popupRoot.popupBottomMargin
            width: popupRoot.cardWidth
            radius: 10
            color: popupRoot.theme.cardBackground
            border.width: 1
            border.color: popupRoot.theme.toolbarDropdownBorder

            implicitHeight: popupRoot.cardMaxHeight > 0
                ? Math.min(popupRoot.cardMaxHeight, popupLayout.implicitHeight + 20)
                : popupLayout.implicitHeight + 20

            data: [
                MouseArea {
                    objectName: popupRoot.objectName + "CardMouseArea"
                    anchors.fill: parent
                    onClicked: mouse.accepted = true
                },
                ColumnLayout {
                    id: popupLayout
                    objectName: popupRoot.objectName + "Layout"
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        objectName: popupRoot.objectName + "Title"
                        Layout.fillWidth: true
                        text: popupRoot.cardTitle
                        color: popupRoot.theme.textPrimary
                        font.family: "Avenir Next"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }
            ]
        }
    ]
}
