import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: dateTimePopup
    readonly property var theme: skyContextController.theme
    required property var skyContextController
    property bool opened: false
    property string stagedDateText: ""
    property string stagedTimeText: ""
    property string errorText: ""
    property real popupRightMargin: 8
    property real popupBottomMargin: 56
    readonly property bool hasInput: stagedDateText.trim() !== "" && stagedTimeText.trim() !== ""

    anchors.fill: parent
    visible: opened
    enabled: opened
    z: 1000

    function syncFromController() {
        stagedDateText = skyContextController.utcDateText
        stagedTimeText = skyContextController.utcTimeText
        errorText = ""
    }

    function open() {
        syncFromController()
        opened = true
        forceActiveFocus()
    }

    function close() {
        opened = false
        errorText = ""
    }

    function applyChanges() {
        const validationError = skyContextController.validateUtcDateTimeText(
            stagedDateText,
            stagedTimeText
        )
        if (validationError !== "") {
            errorText = validationError
            return
        }

        if (skyContextController.setUtcDateTimeText(stagedDateText, stagedTimeText)) {
            close()
            return
        }

        errorText = "Enter a valid UTC date and time."
    }

    Keys.onEscapePressed: close()

    MouseArea {
        anchors.fill: parent
        onClicked: dateTimePopup.close()
    }

    Rectangle {
        id: popupCard
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: dateTimePopup.popupRightMargin
        anchors.bottomMargin: dateTimePopup.popupBottomMargin
        width: 348
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
                text: "Set UTC Date/Time"
                color: theme.textPrimary
                font.family: "Avenir Next"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: "Applies a fixed UTC until you jump back to Now. BCE dates use proleptic Gregorian years."
                color: theme.textSecondary
                font.family: "Avenir Next"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }

            Label {
                text: "UTC Date"
                color: theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
            }

            PreferencesTextField {
                id: dateField
                Layout.fillWidth: true
                placeholderText: "YYYY-MM-DD or YYYY-MM-DD BCE"

                Binding on text {
                    when: !dateField.activeFocus
                    restoreMode: Binding.RestoreNone
                    value: dateTimePopup.stagedDateText
                }

                onTextEdited: {
                    dateTimePopup.stagedDateText = text
                    dateTimePopup.errorText = ""
                }
            }

            Label {
                text: "UTC Time"
                color: theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
            }

            PreferencesTextField {
                id: timeField
                Layout.fillWidth: true
                placeholderText: "HH:mm:ss"

                Binding on text {
                    when: !timeField.activeFocus
                    restoreMode: Binding.RestoreNone
                    value: dateTimePopup.stagedTimeText
                }

                onTextEdited: {
                    dateTimePopup.stagedTimeText = text
                    dateTimePopup.errorText = ""
                }

                Keys.onReturnPressed: dateTimePopup.applyChanges()
                Keys.onEnterPressed: dateTimePopup.applyChanges()
            }

            Label {
                Layout.fillWidth: true
                visible: dateTimePopup.errorText !== ""
                text: dateTimePopup.errorText
                color: theme.errorText
                font.family: "Avenir Next"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }

            Row {
                Layout.alignment: Qt.AlignRight
                spacing: 6

                PreferencesActionButton {
                    width: 72
                    text: "Cancel"
                    onClicked: dateTimePopup.close()
                }

                PreferencesActionButton {
                    width: 64
                    text: "Now"
                    onClicked: {
                        dateTimePopup.errorText = ""
                        dateTimePopup.skyContextController.goLiveNow()
                        dateTimePopup.close()
                    }
                }

                PreferencesActionButton {
                    width: 86
                    primary: true
                    text: "Apply"
                    enabled: dateTimePopup.hasInput
                    onClicked: dateTimePopup.applyChanges()
                }
            }
        }
    }
}
