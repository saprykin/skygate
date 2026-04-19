import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var settingsDraft

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#0f1d39"
        border.width: 1
        border.color: "#2b4b74"

        GridLayout {
            anchors.fill: parent
            anchors.margins: 10
            columns: 4
            rowSpacing: 10
            columnSpacing: 12

            Label {
                text: "Lock UTC Date"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesCheckBox {
                Binding on checked {
                    value: root.settingsDraft.utcDateLocked
                }
                onToggled: root.settingsDraft.utcDateLocked = checked
            }

            Label {
                text: "UTC Date"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesTextField {
                id: utcDateInput
                Layout.fillWidth: true
                placeholderText: "YYYY-MM-DD"
                enabled: !root.settingsDraft.utcDateLocked

                Binding on text {
                    when: !utcDateInput.activeFocus
                    value: root.settingsDraft.utcDateText
                }

                onTextEdited: root.settingsDraft.utcDateText = text
            }

            Label {
                text: "Lock UTC Time"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesCheckBox {
                Binding on checked {
                    value: root.settingsDraft.utcTimeLocked
                }
                onToggled: root.settingsDraft.utcTimeLocked = checked
            }

            Label {
                text: "UTC Time"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesTextField {
                id: utcTimeInput
                Layout.fillWidth: true
                placeholderText: "HH:MM:SS"
                enabled: !root.settingsDraft.utcTimeLocked

                Binding on text {
                    when: !utcTimeInput.activeFocus
                    value: root.settingsDraft.utcTimeText
                }

                onTextEdited: root.settingsDraft.utcTimeText = text
            }

            Label {
                text: "Latitude"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesTextField {
                id: latitudeInput
                Layout.fillWidth: true
                Layout.columnSpan: 3
                placeholderText: "-90..90"
                validator: DoubleValidator {
                    bottom: -90.0
                    top: 90.0
                    notation: DoubleValidator.StandardNotation
                }

                Binding on text {
                    when: !latitudeInput.activeFocus
                    value: root.settingsDraft.latitudeText
                }

                onTextEdited: root.settingsDraft.latitudeText = text
            }

            Label {
                text: "Longitude"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesTextField {
                id: longitudeInput
                Layout.fillWidth: true
                Layout.columnSpan: 3
                placeholderText: "-180..180"
                validator: DoubleValidator {
                    bottom: -180.0
                    top: 180.0
                    notation: DoubleValidator.StandardNotation
                }

                Binding on text {
                    when: !longitudeInput.activeFocus
                    value: root.settingsDraft.longitudeText
                }

                onTextEdited: root.settingsDraft.longitudeText = text
            }

            Label {
                text: "Elevation (m)"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesTextField {
                id: elevationInput
                Layout.fillWidth: true
                Layout.columnSpan: 3
                placeholderText: "meters"
                validator: DoubleValidator {
                    notation: DoubleValidator.StandardNotation
                }

                Binding on text {
                    when: !elevationInput.activeFocus
                    value: root.settingsDraft.elevationText
                }

                onTextEdited: root.settingsDraft.elevationText = text
            }

            Label {
                text: "Projection"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            PreferencesComboBox {
                id: projectionCombo
                Layout.fillWidth: true
                Layout.columnSpan: 3
                model: ["Stereographic", "AzimuthalEquidistant", "Perspective"]

                Binding on currentIndex {
                    value: Math.max(0, projectionCombo.model.indexOf(root.settingsDraft.projectionTypeText))
                }

                onActivated: root.settingsDraft.projectionTypeText = currentText
            }
        }
    }
}
