import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var preferencesDraft

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
            model: root.preferencesDraft.skyContextController.locationSourceOptions

            Binding on currentIndex {
                value: Math.max(
                    0,
                    locationSourceCombo.model.indexOf(root.preferencesDraft.locationSourceText)
                )
            }

            onActivated: root.preferencesDraft.setLocationSource(currentText)
        }

        Label {
            text: "City"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
            visible: root.preferencesDraft.locationSourceText === "City"
        }

        PreferencesCityPicker {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            visible: root.preferencesDraft.locationSourceText === "City"
            enabled: visible
            placeholderText: "Choose a city"
            catalogModel: root.preferencesDraft.skyContextController.cityCatalogModel
            selectedCityId: root.preferencesDraft.selectedCityId
            selectedText: root.preferencesDraft.selectedCityDisplayText
            onCityChosen: function(cityId, displayText, latitudeDeg, longitudeDeg) {
                root.preferencesDraft.selectCity(cityId, displayText, latitudeDeg, longitudeDeg)
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
            catalogModel: root.preferencesDraft.skyContextController.time.timeZoneCatalogModel
            selectedTimeZoneId: root.preferencesDraft.timeZoneId
            selectedText: root.preferencesDraft.timeZoneDisplayText
            onTimeZoneChosen: function(timeZoneId, displayText) {
                root.preferencesDraft.selectTimeZone(timeZoneId, displayText)
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
                value: root.preferencesDraft.latitudeText
            }

            onTextEdited: {
                root.preferencesDraft.useCustomCoordinates()
                root.preferencesDraft.latitudeText = text
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
                value: root.preferencesDraft.longitudeText
            }

            onTextEdited: {
                root.preferencesDraft.useCustomCoordinates()
                root.preferencesDraft.longitudeText = text
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
                value: root.preferencesDraft.elevationText
            }

            onTextEdited: root.preferencesDraft.elevationText = text
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
                value: Math.max(0, projectionCombo.model.indexOf(root.preferencesDraft.projectionTypeText))
            }

            onActivated: root.preferencesDraft.projectionTypeText = currentText
        }
    }
}
