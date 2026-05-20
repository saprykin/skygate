import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

FocusScope {
    id: degradationPopup
    objectName: "degradationPopup"
    readonly property var theme: skyContextController.theme
    required property var skyContextController
    required property var sceneModel
    property bool opened: false
    property real popupRightMargin: 8
    property real popupBottomMargin: 56
    property real scrimTopMargin: 0
    readonly property int reasonCount: sceneModel.ephemerisDegradationReasons
        ? sceneModel.ephemerisDegradationReasons.length
        : 0

    anchors.fill: parent
    visible: opened
    enabled: opened
    z: 1000

    function open() {
        opened = true
        forceActiveFocus()
    }

    function cancel() {
        close()
    }

    function close() {
        opened = false
    }

    Keys.onEscapePressed: close()

    onReasonCountChanged: {
        if (opened && reasonCount === 0) {
            close()
        }
    }

    MouseArea {
        objectName: "degradationPopupScrim"
        anchors.fill: parent
        anchors.topMargin: degradationPopup.scrimTopMargin
        onClicked: degradationPopup.close()
    }

    Rectangle {
        id: popupCard
        objectName: "degradationPopupCard"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: degradationPopup.popupRightMargin
        anchors.bottomMargin: degradationPopup.popupBottomMargin
        width: 360
        radius: 10
        color: theme.cardBackground
        border.width: 1
        border.color: theme.toolbarDropdownBorder
        implicitHeight: Math.min(280, popupLayout.implicitHeight + 20)

        MouseArea {
            objectName: "degradationPopupCardMouseArea"
            anchors.fill: parent
            onClicked: mouse.accepted = true
        }

        ColumnLayout {
            id: popupLayout
            objectName: "degradationPopupLayout"
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Label {
                objectName: "degradationPopupTitle"
                Layout.fillWidth: true
                text: "Ephemeris Degraded"
                color: theme.textPrimary
                font.family: "Avenir Next"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Flickable {
                objectName: "degradationPopupReasonFlickable"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220, reasonsColumn.implicitHeight)
                contentWidth: width
                contentHeight: reasonsColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: reasonsColumn
                    objectName: "degradationPopupReasonColumn"
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: degradationPopup.sceneModel.ephemerisDegradationReasons

                        RowLayout {
                            objectName: "degradationPopupReasonRow"
                            width: reasonsColumn.width
                            spacing: 7

                            Label {
                                objectName: "degradationPopupReasonBullet"
                                text: "!"
                                color: theme.errorText
                                font.family: "Avenir Next"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 1
                            }

                            Label {
                                objectName: "degradationPopupReasonText"
                                Layout.fillWidth: true
                                text: modelData
                                color: theme.textSecondary
                                font.family: "Avenir Next"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
