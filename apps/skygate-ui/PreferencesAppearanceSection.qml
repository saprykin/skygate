import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var preferencesDraft
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
                columnSpan: 4
                text: "Appearance"
            }

            Label {
                text: "Theme"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesComboBox {
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
                columnSpan: 4
                text: "Overlay Layers"
            }

            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayHorizon
                onToggled: root.preferencesDraft.overlayHorizon = checked
            }
            Label {
                text: "Horizon"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayAltAzGrid
                onToggled: root.preferencesDraft.overlayAltAzGrid = checked
            }
            Label {
                text: "Alt/Az grid"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayConstellationLines
                onToggled: root.preferencesDraft.overlayConstellationLines = checked
            }
            Label {
                text: "Constellation lines"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayConstellationLabels
                onToggled: root.preferencesDraft.overlayConstellationLabels = checked
            }
            Label {
                text: "Constellation labels"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.preferencesDraft.overlaySolarSystemLabels
                onToggled: root.preferencesDraft.overlaySolarSystemLabels = checked
            }
            Label {
                text: "Solar system labels"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayDeepSkyObjects
                onToggled: root.preferencesDraft.overlayDeepSkyObjects = checked
            }
            Label {
                text: "Deep sky objects"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayDeepSkyLabels
                onToggled: root.preferencesDraft.overlayDeepSkyLabels = checked
            }
            Label {
                text: "Deep sky labels"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayEcliptic
                onToggled: root.preferencesDraft.overlayEcliptic = checked
            }
            Label {
                text: "Ecliptic"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayCelestialEquator
                onToggled: root.preferencesDraft.overlayCelestialEquator = checked
            }
            Label {
                text: "Celestial equator"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.preferencesDraft.overlayCircumpolarBoundary
                onToggled: root.preferencesDraft.overlayCircumpolarBoundary = checked
            }
            Label {
                text: "Circumpolar boundary"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
