import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: root
    required property var preferencesDraft
    required property var theme
    readonly property var themeOptions: preferencesDraft.skyContextController.themeOptions

    function themeIndexForId(themeId) {
        for (let index = 0; index < themeOptions.length; ++index) {
            if (themeOptions[index].id === themeId) {
                return index
            }
        }
        return 0
    }

    function themeLabels() {
        const labels = []
        for (let index = 0; index < themeOptions.length; ++index) {
            labels.push(themeOptions[index].label)
        }
        return labels
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 3
            columnSpacing: 6

            PreferencesGroupTitle {
                theme: root.theme
                columnSpan: 4
                text: "Appearance"
            }

            PreferencesFieldLabel {
                theme: root.theme
                text: "Theme"
                textPixelSize: 10
            }

            PreferencesComboBox {
                theme: root.theme
                id: themeCombo
                objectName: "themeCombo"
                Layout.fillWidth: true
                Layout.columnSpan: 3
                model: root.themeLabels()

                Binding on currentIndex {
                    value: root.themeIndexForId(root.preferencesDraft.themeId)
                }

                onActivated: {
                    if (currentIndex >= 0 && currentIndex < root.themeOptions.length) {
                        root.preferencesDraft.themeId = root.themeOptions[currentIndex].id
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 7
            columnSpacing: 8

            PreferencesGroupTitle {
                theme: root.theme
                columnSpan: 4
                text: "Overlay Layers"
            }

            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayHorizonCheckBox"
                checked: root.preferencesDraft.overlayHorizon
                onToggled: root.preferencesDraft.overlayHorizon = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Horizon"
            }
            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayAltAzGridCheckBox"
                checked: root.preferencesDraft.overlayAltAzGrid
                onToggled: root.preferencesDraft.overlayAltAzGrid = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Alt/Az grid"
            }

            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayConstellationLinesCheckBox"
                checked: root.preferencesDraft.overlayConstellationLines
                onToggled: root.preferencesDraft.overlayConstellationLines = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Constellation lines"
            }
            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayConstellationLabelsCheckBox"
                checked: root.preferencesDraft.overlayConstellationLabels
                onToggled: root.preferencesDraft.overlayConstellationLabels = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Constellation labels"
            }

            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlaySolarSystemLabelsCheckBox"
                checked: root.preferencesDraft.overlaySolarSystemLabels
                onToggled: root.preferencesDraft.overlaySolarSystemLabels = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Solar system labels"
            }
            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayDeepSkyObjectsCheckBox"
                checked: root.preferencesDraft.overlayDeepSkyObjects
                onToggled: root.preferencesDraft.overlayDeepSkyObjects = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Deep sky objects"
            }

            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayDeepSkyLabelsCheckBox"
                checked: root.preferencesDraft.overlayDeepSkyLabels
                onToggled: root.preferencesDraft.overlayDeepSkyLabels = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Deep sky labels"
            }
            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayEclipticCheckBox"
                checked: root.preferencesDraft.overlayEcliptic
                onToggled: root.preferencesDraft.overlayEcliptic = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Ecliptic"
            }

            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayCelestialEquatorCheckBox"
                checked: root.preferencesDraft.overlayCelestialEquator
                onToggled: root.preferencesDraft.overlayCelestialEquator = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Celestial equator"
            }
            PreferencesCheckBox {
                theme: root.theme
                objectName: "overlayCircumpolarBoundaryCheckBox"
                checked: root.preferencesDraft.overlayCircumpolarBoundary
                onToggled: root.preferencesDraft.overlayCircumpolarBoundary = checked
            }
            PreferencesFieldLabel {
                theme: root.theme
                text: "Circumpolar boundary"
            }
        }
    }
}
