import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

StatusPopupScaffold {
    id: dateTimePopup
    objectName: "dateTimePopup"
    cardWidth: 348
    cardTitle: "Set Date/Time"

    property string stagedDateText: ""
    property string stagedTimeText: ""
    property string errorText: ""
    readonly property bool hasInput: stagedDateText.trim() !== "" && stagedTimeText.trim() !== ""

    onOpening: syncFromController()
    onClosing: errorText = ""

    function syncFromController() {
        stagedDateText = skyContextController.time.dateText
        stagedTimeText = skyContextController.time.timeText
        errorText = ""
    }

    function applyChanges() {
        const validationError = skyContextController.time.validateDateTimeText(
            stagedDateText,
            stagedTimeText
        )
        if (validationError !== "") {
            errorText = validationError
            return
        }

        if (skyContextController.time.setDateTimeText(stagedDateText, stagedTimeText)) {
            close()
            return
        }

        errorText = "Enter a valid date and time."
    }

    Label {
        objectName: "dateTimePopupTimeZoneDescription"
        Layout.fillWidth: true
        text: "Interpreted as "
              + dateTimePopup.skyContextController.time.timeZoneId
              + " · "
              + dateTimePopup.skyContextController.time.timeZoneLabel
              + ". BCE dates use proleptic Gregorian years."
        color: theme.textSecondary
        font.family: "Avenir Next"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }

    PreferencesFieldLabel {
        theme: dateTimePopup.theme
        objectName: "dateTimePopupDateLabel"
        text: "Date"
        textPixelSize: 10
    }

    PreferencesTextField {
        theme: dateTimePopup.theme
        id: dateField
        objectName: "dateTimePopupDateField"
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

        onAccepted: dateTimePopup.applyChanges()
    }

    PreferencesFieldLabel {
        theme: dateTimePopup.theme
        objectName: "dateTimePopupTimeLabel"
        text: "Time"
        textPixelSize: 10
    }

    PreferencesTextField {
        theme: dateTimePopup.theme
        id: timeField
        objectName: "dateTimePopupTimeField"
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

        onAccepted: dateTimePopup.applyChanges()
    }

    Label {
        objectName: "dateTimePopupErrorLabel"
        Layout.fillWidth: true
        visible: dateTimePopup.errorText !== ""
        text: dateTimePopup.errorText
        color: theme.errorText
        font.family: "Avenir Next"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }

    Row {
        objectName: "dateTimePopupActionsRow"
        Layout.alignment: Qt.AlignRight
        spacing: 6

        PreferencesActionButton {
            theme: dateTimePopup.theme
            objectName: "dateTimePopupCancelButton"
            width: 72
            text: "Cancel"
            onClicked: dateTimePopup.cancel()
        }

        PreferencesActionButton {
            theme: dateTimePopup.theme
            objectName: "dateTimePopupNowButton"
            width: 64
            text: "Now"
            onClicked: {
                dateTimePopup.errorText = ""
                dateTimePopup.skyContextController.time.goLiveNow()
                dateTimePopup.close()
            }
        }

        PreferencesActionButton {
            theme: dateTimePopup.theme
            objectName: "dateTimePopupApplyButton"
            width: 86
            primary: true
            text: "Apply"
            enabled: dateTimePopup.hasInput
            onClicked: dateTimePopup.applyChanges()
        }
    }
}
