import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: root
    required property var preferencesDraft
    required property var theme
    readonly property var themeOptions: preferencesDraft.skyContextController.themeOptions
    readonly property var overlayRows: [
        {
            objectName: "overlayHorizonCheckBox",
            label: "Horizon",
            draftProperty: "overlayHorizon"
        },
        {
            objectName: "overlayAltAzGridCheckBox",
            label: "Alt/Az grid",
            draftProperty: "overlayAltAzGrid"
        },
        {
            objectName: "overlayConstellationLinesCheckBox",
            label: "Constellation lines",
            draftProperty: "overlayConstellationLines"
        },
        {
            objectName: "overlayConstellationLabelsCheckBox",
            label: "Constellation labels",
            draftProperty: "overlayConstellationLabels"
        },
        {
            objectName: "overlaySolarSystemLabelsCheckBox",
            label: "Solar system labels",
            draftProperty: "overlaySolarSystemLabels"
        },
        {
            objectName: "overlayDeepSkyObjectsCheckBox",
            label: "Deep sky objects",
            draftProperty: "overlayDeepSkyObjects"
        },
        {
            objectName: "overlayDeepSkyLabelsCheckBox",
            label: "Deep sky labels",
            draftProperty: "overlayDeepSkyLabels"
        },
        {
            objectName: "overlayEclipticCheckBox",
            label: "Ecliptic",
            draftProperty: "overlayEcliptic"
        },
        {
            objectName: "overlayCelestialEquatorCheckBox",
            label: "Celestial equator",
            draftProperty: "overlayCelestialEquator"
        },
        {
            objectName: "overlayCircumpolarBoundaryCheckBox",
            label: "Circumpolar boundary",
            draftProperty: "overlayCircumpolarBoundary"
        }
    ]

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
                Layout.row: 0
                Layout.column: 0
                columnSpan: 4
                text: "Overlay Layers"
            }

            Repeater {
                model: root.overlayRows

                PreferencesCheckBox {
                    required property var modelData
                    required property int index

                    theme: root.theme
                    Layout.row: Math.floor(index / 2) + 1
                    Layout.column: (index % 2) * 2
                    objectName: modelData.objectName
                    checked: root.preferencesDraft[modelData.draftProperty]
                    onToggled: root.preferencesDraft[modelData.draftProperty] = checked
                }
            }

            Repeater {
                model: root.overlayRows

                PreferencesFieldLabel {
                    required property var modelData
                    required property int index

                    theme: root.theme
                    Layout.row: Math.floor(index / 2) + 1
                    Layout.column: (index % 2) * 2 + 1
                    text: modelData.label
                }
            }
        }
    }
}
