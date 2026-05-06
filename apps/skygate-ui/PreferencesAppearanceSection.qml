import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var settingsDraft
    readonly property var themeOptions: settingsDraft.skyContextController.themeOptions

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
                    value: root.themeIndexForId(root.settingsDraft.themeId)
                }

                onActivated: {
                    if (currentIndex >= 0 && currentIndex < root.themeOptions.length) {
                        root.settingsDraft.themeId = root.themeOptions[currentIndex].id
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
                checked: root.settingsDraft.overlayHorizon
                onToggled: root.settingsDraft.overlayHorizon = checked
            }
            Label {
                text: "Horizon"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.settingsDraft.overlayAltAzGrid
                onToggled: root.settingsDraft.overlayAltAzGrid = checked
            }
            Label {
                text: "Alt/Az grid"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.settingsDraft.overlayConstellationLines
                onToggled: root.settingsDraft.overlayConstellationLines = checked
            }
            Label {
                text: "Constellation lines"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.settingsDraft.overlayConstellationLabels
                onToggled: root.settingsDraft.overlayConstellationLabels = checked
            }
            Label {
                text: "Constellation labels"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.settingsDraft.overlaySolarSystemLabels
                onToggled: root.settingsDraft.overlaySolarSystemLabels = checked
            }
            Label {
                text: "Solar system labels"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.settingsDraft.overlayDeepSkyObjects
                onToggled: root.settingsDraft.overlayDeepSkyObjects = checked
            }
            Label {
                text: "Deep sky objects"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.settingsDraft.overlayDeepSkyLabels
                onToggled: root.settingsDraft.overlayDeepSkyLabels = checked
            }
            Label {
                text: "Deep sky labels"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.settingsDraft.overlayEcliptic
                onToggled: root.settingsDraft.overlayEcliptic = checked
            }
            Label {
                text: "Ecliptic"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            PreferencesCheckBox {
                checked: root.settingsDraft.overlayCelestialEquator
                onToggled: root.settingsDraft.overlayCelestialEquator = checked
            }
            Label {
                text: "Celestial equator"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }
            PreferencesCheckBox {
                checked: root.settingsDraft.overlayCircumpolarBoundary
                onToggled: root.settingsDraft.overlayCircumpolarBoundary = checked
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
