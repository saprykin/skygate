import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: nightConditionsPopup
    readonly property var theme: skyContextController.theme
    readonly property var conditions: skyContextController.nightConditions
    required property var skyContextController
    property bool opened: false
    property real popupRightMargin: 8
    property real popupBottomMargin: 56

    anchors.fill: parent
    visible: opened
    enabled: opened
    z: 1000

    function open() {
        skyContextController.refreshNightConditions()
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

    Timer {
        interval: 60000
        repeat: true
        running: nightConditionsPopup.opened
        onTriggered: nightConditionsPopup.skyContextController.refreshNightConditions()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: nightConditionsPopup.close()
    }

    Rectangle {
        id: popupCard
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: nightConditionsPopup.popupRightMargin
        anchors.bottomMargin: nightConditionsPopup.popupBottomMargin
        width: 244
        radius: 10
        color: theme.cardBackground
        border.width: 1
        border.color: theme.toolbarDropdownBorder

        implicitHeight: popupLayout.implicitHeight + 20

        MouseArea {
            anchors.fill: parent
            onClicked: mouse.accepted = true
        }

        ColumnLayout {
            id: popupLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: "Night Conditions"
                color: theme.textPrimary
                font.family: "Avenir Next"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: nightConditionsPopup.conditions.locationText || "UTC"
                color: theme.textSecondary
                font.family: "Avenir Next"
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: theme.dividerColor
            }

            Label {
                Layout.fillWidth: true
                text: "Sun"
                color: theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Repeater {
                model: nightConditionsPopup.conditions.sunRows || []

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: modelData.label
                        color: theme.textSecondary
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Label {
                        text: modelData.value
                        color: theme.textPrimary
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: theme.dividerColor
            }

            Label {
                Layout.fillWidth: true
                text: "Moon"
                color: theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: nightConditionsPopup.conditions.moonPhaseText || "--"
                color: theme.textPrimary
                font.family: "Avenir Next"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "Rise"
                    color: theme.textSecondary
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                }

                Label {
                    text: nightConditionsPopup.conditions.moonRiseText || "--"
                    color: theme.textPrimary
                    font.family: "Menlo"
                    font.pixelSize: 11
                }

                Label {
                    Layout.leftMargin: 10
                    text: "Set"
                    color: theme.textSecondary
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                }

                Label {
                    text: nightConditionsPopup.conditions.moonSetText || "--"
                    color: theme.textPrimary
                    font.family: "Menlo"
                    font.pixelSize: 11
                }
            }

            Label {
                Layout.fillWidth: true
                visible: nightConditionsPopup.conditions.valid === false
                text: "Conditions unavailable for the current location or catalog."
                color: theme.textMuted
                font.family: "Avenir Next"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
    }
}
