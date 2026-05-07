import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: nightConditionsPopup
    objectName: "nightConditionsPopup"
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
        objectName: "nightConditionsPopupRefreshTimer"
        interval: 60000
        repeat: true
        running: nightConditionsPopup.opened
        onTriggered: nightConditionsPopup.skyContextController.refreshNightConditions()
    }

    MouseArea {
        objectName: "nightConditionsPopupScrim"
        anchors.fill: parent
        onClicked: nightConditionsPopup.close()
    }

    Rectangle {
        id: popupCard
        objectName: "nightConditionsPopupCard"
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
            objectName: "nightConditionsPopupCardMouseArea"
            anchors.fill: parent
            onClicked: mouse.accepted = true
        }

        ColumnLayout {
            id: popupLayout
            objectName: "nightConditionsPopupLayout"
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Label {
                objectName: "nightConditionsPopupTitle"
                Layout.fillWidth: true
                text: "Night Conditions"
                color: theme.textPrimary
                font.family: "Avenir Next"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Label {
                objectName: "nightConditionsLocationLabel"
                Layout.fillWidth: true
                text: nightConditionsPopup.conditions.locationText || "UTC"
                color: theme.textSecondary
                font.family: "Avenir Next"
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            Rectangle {
                objectName: "nightConditionsSunDivider"
                Layout.fillWidth: true
                height: 1
                color: theme.dividerColor
            }

            Label {
                objectName: "nightConditionsSunHeader"
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
                    objectName: "nightConditionsSunRow_" + modelData.label
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        objectName: "nightConditionsSunRowLabel_" + modelData.label
                        Layout.fillWidth: true
                        text: modelData.label
                        color: theme.textSecondary
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Label {
                        objectName: "nightConditionsSunRowValue_" + modelData.label
                        text: modelData.value
                        color: theme.textPrimary
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                objectName: "nightConditionsMoonDivider"
                Layout.fillWidth: true
                height: 1
                color: theme.dividerColor
            }

            Label {
                objectName: "nightConditionsMoonHeader"
                Layout.fillWidth: true
                text: "Moon"
                color: theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Label {
                objectName: "nightConditionsMoonPhaseLabel"
                Layout.fillWidth: true
                text: nightConditionsPopup.conditions.moonPhaseText || "--"
                color: theme.textPrimary
                font.family: "Avenir Next"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            ColumnLayout {
                objectName: "nightConditionsMoonRiseSetRows"
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    objectName: "nightConditionsMoonRiseRow"
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        objectName: "nightConditionsMoonRiseLabel"
                        Layout.fillWidth: true
                        text: "Rise"
                        color: theme.textSecondary
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                    }

                    Label {
                        objectName: "nightConditionsMoonRiseValue"
                        text: nightConditionsPopup.conditions.moonRiseText || "--"
                        color: theme.textPrimary
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }

                RowLayout {
                    objectName: "nightConditionsMoonSetRow"
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        objectName: "nightConditionsMoonSetLabel"
                        Layout.fillWidth: true
                        text: "Set"
                        color: theme.textSecondary
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                    }

                    Label {
                        objectName: "nightConditionsMoonSetValue"
                        text: nightConditionsPopup.conditions.moonSetText || "--"
                        color: theme.textPrimary
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }
            }

            Label {
                objectName: "nightConditionsUnavailableLabel"
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
