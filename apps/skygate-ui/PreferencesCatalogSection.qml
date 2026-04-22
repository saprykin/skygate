import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: catalogSection
    required property var skyContextController
    required property var settingsDraft
    readonly property bool catalogBusy: skyContextController.downloadingCatalog
                                       || skyContextController.catalogProcessing

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 6
            columnSpacing: 8

            PreferencesComboBox {
                id: catalogPresetCombo
                Layout.fillWidth: true
                model: [
                    "Bundled (recommended)",
                    "HYG v4.2 stars + Stellarium lines",
                    "Custom URL"
                ]

                Binding on currentIndex {
                    value: Math.max(0, Math.min(catalogPresetCombo.count - 1, settingsDraft.catalogPresetIndex))
                }

                onActivated: settingsDraft.catalogPresetIndex = currentIndex
            }

            PreferencesActionButton {
                Layout.preferredWidth: 150
                text: "Use Preset"
                enabled: catalogPresetCombo.currentIndex !== 2
                onClicked: {
                    settingsDraft.catalogPresetIndex = catalogPresetCombo.currentIndex
                    if (catalogPresetCombo.currentIndex === 0) {
                        skyContextController.loadCatalogPreset("bundled")
                    } else if (catalogPresetCombo.currentIndex === 1) {
                        settingsDraft.catalogUrlText =
                            "https://www.astronexus.com/downloads/catalogs/hygdata_v42.csv.gz"
                        skyContextController.loadCatalogPreset("hyg_v42")
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: skyContextController.catalogDatasetInfoText
                color: skyContext.theme.listItemPrimaryText
                font.pixelSize: 11
                font.family: "Avenir Next"
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            PreferencesActionButton {
                Layout.preferredWidth: 150
                Layout.alignment: Qt.AlignVCenter
                text: "Clear Catalog Cache"
                enabled: !skyContextController.downloadingCatalog
                         && !skyContextController.catalogProcessing
                onClicked: skyContextController.clearCatalogCache()
            }

            Label {
                visible: catalogPresetCombo.currentIndex === 2
                Layout.columnSpan: 2
                Layout.preferredHeight: visible ? implicitHeight : 0
                text: "Catalog URL"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
            }

            PreferencesTextField {
                id: catalogUrlInput
                visible: catalogPresetCombo.currentIndex === 2
                Layout.preferredHeight: visible ? implicitHeight : 0
                Layout.fillWidth: true
                placeholderText: "https://example.com/skygate-catalog.txt or HYG CSV URL"
                Component.onCompleted: cursorPosition = 0

                Binding on text {
                    when: !catalogUrlInput.activeFocus
                    restoreMode: Binding.RestoreNone
                    value: settingsDraft.catalogUrlText
                }

                onTextEdited: settingsDraft.catalogUrlText = text
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        cursorPosition = 0
                    }
                }
                onTextChanged: {
                    if (!activeFocus) {
                        cursorPosition = 0
                    }
                }
            }

            PreferencesActionButton {
                visible: catalogPresetCombo.currentIndex === 2
                Layout.preferredWidth: 150
                Layout.preferredHeight: visible ? implicitHeight : 0
                text: skyContextController.downloadingCatalog ? "Downloading..." : "Download"
                enabled: !skyContextController.downloadingCatalog
                onClicked: {
                    settingsDraft.catalogPresetIndex = 2
                    skyContextController.downloadCatalogFromUrl(settingsDraft.catalogUrlText)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
        }
    }
}
