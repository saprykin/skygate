import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var settingsDraft

    GridLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        columns: 4
        rowSpacing: 3
        columnSpacing: 6

        PreferencesGroupTitle {
            columnSpan: 4
            text: "Location"
        }

        Label {
            text: "Location Source"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
        }

        PreferencesComboBox {
            id: locationSourceCombo
            Layout.fillWidth: true
            Layout.columnSpan: 3
            model: root.settingsDraft.skyContextController.locationSourceOptions

            Binding on currentIndex {
                value: Math.max(
                    0,
                    locationSourceCombo.model.indexOf(root.settingsDraft.locationSourceText)
                )
            }

            onActivated: root.settingsDraft.setLocationSource(currentText)
        }

        Label {
            text: "City"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
            visible: root.settingsDraft.locationSourceText === "City"
        }

        PreferencesCityPicker {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            visible: root.settingsDraft.locationSourceText === "City"
            enabled: visible
            placeholderText: "Choose a city"
            catalogModel: root.settingsDraft.skyContextController.cityCatalogModel
            selectedCityId: root.settingsDraft.selectedCityId
            selectedText: root.settingsDraft.selectedCityDisplayText
            onCityChosen: function(cityId, displayText, latitudeDeg, longitudeDeg) {
                root.settingsDraft.selectCity(cityId, displayText, latitudeDeg, longitudeDeg)
            }
        }

        Label {
            text: "Timezone"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
        }

        PreferencesTimeZonePicker {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            placeholderText: "Choose a timezone"
            catalogModel: root.settingsDraft.skyContextController.time.timeZoneCatalogModel
            selectedTimeZoneId: root.settingsDraft.timeZoneId
            selectedText: root.settingsDraft.timeZoneDisplayText
            onTimeZoneChosen: function(timeZoneId, displayText) {
                root.settingsDraft.selectTimeZone(timeZoneId, displayText)
            }
        }

        Label {
            text: "Latitude"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
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
                restoreMode: Binding.RestoreNone
                value: root.settingsDraft.latitudeText
            }

            onTextEdited: {
                root.settingsDraft.useCustomCoordinates()
                root.settingsDraft.latitudeText = text
            }
        }

        Label {
            text: "Longitude"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
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
                restoreMode: Binding.RestoreNone
                value: root.settingsDraft.longitudeText
            }

            onTextEdited: {
                root.settingsDraft.useCustomCoordinates()
                root.settingsDraft.longitudeText = text
            }
        }

        Label {
            text: "Elevation (m)"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 10
            Layout.alignment: Qt.AlignVCenter
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
                restoreMode: Binding.RestoreNone
                value: root.settingsDraft.elevationText
            }

            onTextEdited: root.settingsDraft.elevationText = text
        }

        PreferencesGroupTitle {
            columnSpan: 4
            Layout.topMargin: 8
            text: "View"
        }

        Label {
            text: "Projection"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 10
            Layout.alignment: Qt.AlignVCenter
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
