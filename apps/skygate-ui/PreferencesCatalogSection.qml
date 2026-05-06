import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: catalogSection
    required property var skyContextController
    required property var preferencesDraft
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

            PreferencesGroupTitle {
                columnSpan: 2
                text: "Star Catalog"
            }

            PreferencesComboBox {
                id: catalogPresetCombo
                Layout.fillWidth: true
                model: [
                    "Bundled (recommended)",
                    "HYG v4.2 stars + Stellarium lines",
                    "Custom URL"
                ]

                Binding on currentIndex {
                    value: Math.max(0, Math.min(catalogPresetCombo.count - 1, preferencesDraft.catalogPresetIndex))
                }

                onActivated: preferencesDraft.catalogPresetIndex = currentIndex
            }

            PreferencesActionButton {
                Layout.preferredWidth: 150
                text: "Use catalog"
                enabled: !catalogBusy && catalogPresetCombo.currentIndex !== 2
                onClicked: {
                    preferencesDraft.catalogPresetIndex = catalogPresetCombo.currentIndex
                    if (catalogPresetCombo.currentIndex === 0) {
                        skyContextController.loadCatalogPreset("bundled")
                    } else if (catalogPresetCombo.currentIndex === 1) {
                        preferencesDraft.catalogUrlText =
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
                onClicked: {
                    preferencesDraft.catalogPresetIndex = 0
                    skyContextController.loadCatalogPreset("bundled")
                    skyContextController.clearCatalogCache()
                }
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
                    value: preferencesDraft.catalogUrlText
                }

                onTextEdited: preferencesDraft.catalogUrlText = text
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
                enabled: !catalogBusy
                onClicked: {
                    preferencesDraft.catalogPresetIndex = 2
                    skyContextController.downloadCatalogFromUrl(preferencesDraft.catalogUrlText)
                }
            }

            PreferencesGroupTitle {
                columnSpan: 2
                Layout.topMargin: 8
                text: "Deep-Sky Catalog"
            }

            PreferencesComboBox {
                id: deepSkyCatalogPresetCombo
                Layout.fillWidth: true
                model: [
                    "Bundled Messier",
                    "OpenNGC",
                    "Custom URL"
                ]

                Binding on currentIndex {
                    value: Math.max(
                        0,
                        Math.min(
                            deepSkyCatalogPresetCombo.count - 1,
                            preferencesDraft.deepSkyCatalogPresetIndex
                        )
                    )
                }

                onActivated: preferencesDraft.deepSkyCatalogPresetIndex = currentIndex
            }

            PreferencesActionButton {
                Layout.preferredWidth: 150
                text: "Use catalog"
                enabled: !catalogBusy && deepSkyCatalogPresetCombo.currentIndex !== 2
                onClicked: {
                    preferencesDraft.deepSkyCatalogPresetIndex = deepSkyCatalogPresetCombo.currentIndex
                    if (deepSkyCatalogPresetCombo.currentIndex === 0) {
                        skyContextController.loadDeepSkyCatalogPreset("bundled_messier")
                    } else {
                        skyContextController.loadDeepSkyCatalogPreset("open_ngc")
                        preferencesDraft.deepSkyCatalogUrlText =
                            skyContextController.deepSkyCatalogUrlText()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: skyContextController.deepSkyCatalogInfoText
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
                enabled: !catalogBusy
                onClicked: {
                    preferencesDraft.deepSkyCatalogPresetIndex = 0
                    skyContextController.loadDeepSkyCatalogPreset("bundled_messier")
                    skyContextController.clearDeepSkyCatalogCache()
                }
            }

            Label {
                visible: deepSkyCatalogPresetCombo.currentIndex === 2
                Layout.columnSpan: 2
                Layout.preferredHeight: visible ? implicitHeight : 0
                text: "Catalog URL"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 11
            }

            PreferencesTextField {
                id: deepSkyCatalogUrlInput
                visible: deepSkyCatalogPresetCombo.currentIndex === 2
                Layout.preferredHeight: visible ? implicitHeight : 0
                Layout.fillWidth: true
                placeholderText: "https://raw.githubusercontent.com/mattiaverga/OpenNGC/.../NGC.csv"
                Component.onCompleted: cursorPosition = 0

                Binding on text {
                    when: !deepSkyCatalogUrlInput.activeFocus
                    restoreMode: Binding.RestoreNone
                    value: preferencesDraft.deepSkyCatalogUrlText
                }

                onTextEdited: preferencesDraft.deepSkyCatalogUrlText = text
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
                visible: deepSkyCatalogPresetCombo.currentIndex === 2
                Layout.preferredWidth: 150
                Layout.preferredHeight: visible ? implicitHeight : 0
                text: skyContextController.downloadingCatalog ? "Downloading..." : "Download"
                enabled: !catalogBusy
                onClicked: {
                    preferencesDraft.deepSkyCatalogPresetIndex = 2
                    skyContextController.downloadDeepSkyCatalogFromUrl(
                        preferencesDraft.deepSkyCatalogUrlText
                    )
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
