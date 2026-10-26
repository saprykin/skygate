import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: root
    required property var preferencesDraft
    required property var theme

    GridLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        columns: 4
        rowSpacing: 3
        columnSpacing: 6

        PreferencesGroupTitle {
            theme: root.theme
            columnSpan: 4
            text: "Location"
        }

        PreferencesFieldLabel {
            theme: root.theme
            text: "Location Source"
        }

        PreferencesComboBox {
            theme: root.theme
            id: locationSourceCombo
            objectName: "locationSourceCombo"
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

        PreferencesFieldLabel {
            theme: root.theme
            visible: root.preferencesDraft.locationSourceText === "City"
            text: "City"
        }

        PreferencesCityPicker {
            theme: root.theme
            objectName: "cityPicker"
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

        PreferencesFieldLabel {
            theme: root.theme
            text: "Timezone"
        }

        PreferencesTimeZonePicker {
            theme: root.theme
            objectName: "timeZonePicker"
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

        PreferencesFieldLabel {
            theme: root.theme
            text: "Latitude"
        }

        PreferencesTextField {
            theme: root.theme
            id: latitudeInput
            objectName: "latitudeInput"
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

        PreferencesFieldLabel {
            theme: root.theme
            text: "Longitude"
        }

        PreferencesTextField {
            theme: root.theme
            id: longitudeInput
            objectName: "longitudeInput"
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

        PreferencesFieldLabel {
            theme: root.theme
            text: "Elevation (m)"
            textPixelSize: 10
        }

        PreferencesTextField {
            theme: root.theme
            id: elevationInput
            objectName: "elevationInput"
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
            theme: root.theme
            columnSpan: 4
            Layout.topMargin: 8
            text: "View"
        }

        PreferencesFieldLabel {
            theme: root.theme
            text: "Projection"
            textPixelSize: 10
        }

        PreferencesComboBox {
            theme: root.theme
            id: projectionCombo
            objectName: "projectionCombo"
            Layout.fillWidth: true
            Layout.columnSpan: 3
            model: root.preferencesDraft.skyContextController.projectionTypeOptions

            Binding on currentIndex {
                value: Math.max(0, projectionCombo.model.indexOf(root.preferencesDraft.projectionTypeText))
            }

            onActivated: root.preferencesDraft.projectionTypeText = currentText
        }
    }
}
