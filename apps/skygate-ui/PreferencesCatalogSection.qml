import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: catalogSection
    required property var skyContextController
    property alias catalogPresetCurrentIndex: catalogPresetCombo.currentIndex
    readonly property int catalogPresetCount: catalogPresetCombo.count
    property alias catalogUrlText: catalogUrlInput.text

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#0f1d39"
        border.width: 1
        border.color: "#2b4b74"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Label {
                text: "Catalog preset"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                PreferencesComboBox {
                    id: catalogPresetCombo
                    Layout.fillWidth: true
                    model: [
                        "Bundled (recommended)",
                        "Starter (bright objects)",
                        "Major constellations",
                        "HYG v3 stars + Stellarium lines",
                        "Custom URL"
                    ]
                }

                PreferencesActionButton {
                    text: "Use Preset"
                    enabled: catalogPresetCombo.currentIndex !== 4
                    onClicked: {
                        if (catalogPresetCombo.currentIndex === 0) {
                            skyContextController.loadCatalogPreset("bundled")
                        } else if (catalogPresetCombo.currentIndex === 1) {
                            skyContextController.loadCatalogPreset("starter")
                        } else if (catalogPresetCombo.currentIndex === 2) {
                            skyContextController.loadCatalogPreset("constellations_major")
                        } else if (catalogPresetCombo.currentIndex === 3) {
                            skyContextController.loadCatalogPreset("hyg_v3")
                            catalogUrlInput.text = "https://raw.githubusercontent.com/astronexus/HYG-Database/master/hygdata_v3.csv"
                        }
                    }
                }
            }

            Label {
                visible: catalogPresetCombo.currentIndex === 4
                Layout.preferredHeight: visible ? implicitHeight : 0
                text: "Catalog URL"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            RowLayout {
                visible: catalogPresetCombo.currentIndex === 4
                Layout.preferredHeight: visible ? implicitHeight : 0
                Layout.fillWidth: true
                spacing: 6

                PreferencesTextField {
                    id: catalogUrlInput
                    Layout.fillWidth: true
                    text: "https://raw.githubusercontent.com/astronexus/HYG-Database/master/hygdata_v3.csv"
                    placeholderText: "https://example.com/skygate-catalog.txt or HYG CSV URL"
                    Component.onCompleted: cursorPosition = 0
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
                    text: skyContextController.downloadingCatalog ? "Downloading..." : "Download"
                    enabled: !skyContextController.downloadingCatalog
                    onClicked: skyContextController.downloadCatalogFromUrl(catalogUrlInput.text)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: 82
                Layout.minimumHeight: 82
                radius: 9
                color: "#0c1830"
                border.width: 1
                border.color: "#2b4a72"

                Label {
                    anchors.fill: parent
                    anchors.margins: 10
                    text: skyContextController.catalogStatusText
                    color: "#9ab0d6"
                    font.pixelSize: 12
                    font.family: "Avenir Next"
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
