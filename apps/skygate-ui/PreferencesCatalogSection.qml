import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: catalogSection
    required property var skyContextController
    required property var settingsDraft

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
                        "HYG v4.2 stars + Stellarium lines",
                        "Custom URL"
                    ]

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(catalogPresetCombo.count - 1, settingsDraft.catalogPresetIndex))
                    }

                    onActivated: settingsDraft.catalogPresetIndex = currentIndex
                }

                PreferencesActionButton {
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
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 82
                radius: 9
                color: "#0c1830"
                border.width: 1
                border.color: "#2b4a72"

                Label {
                    anchors.fill: parent
                    anchors.margins: 10
                    text: skyContextController.catalogDatasetInfoText
                    color: "#9ab0d6"
                    font.pixelSize: 12
                    font.family: "Avenir Next"
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                PreferencesActionButton {
                    text: "Clear Catalog Cache"
                    enabled: !skyContextController.downloadingCatalog
                             && !skyContextController.catalogProcessing
                    onClicked: skyContextController.clearCatalogCache()
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Label {
                visible: catalogPresetCombo.currentIndex === 2
                Layout.preferredHeight: visible ? implicitHeight : 0
                text: "Catalog URL"
                color: "#cad9f7"
                font.family: "Avenir Next"
            }

            RowLayout {
                visible: catalogPresetCombo.currentIndex === 2
                Layout.preferredHeight: visible ? implicitHeight : 0
                Layout.fillWidth: true
                spacing: 6

                PreferencesTextField {
                    id: catalogUrlInput
                    Layout.fillWidth: true
                    placeholderText: "https://example.com/skygate-catalog.txt or HYG CSV URL"
                    Component.onCompleted: cursorPosition = 0

                    Binding on text {
                        when: !catalogUrlInput.activeFocus
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 62 : 0
                Layout.minimumHeight: visible ? 62 : 0
                visible: skyContextController.downloadingCatalog || skyContextController.catalogProcessing
                radius: 10
                color: "#0d2039"
                border.width: 1
                border.color: "#325d86"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: skyContextController.catalogStatusText
                        color: "#b8d8ff"
                        font.pixelSize: 12
                        font.family: "Avenir Next"
                        elide: Text.ElideRight
                    }

                    ProgressBar {
                        id: catalogProgressBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 12
                        from: 0
                        to: 1
                        value: 0
                        indeterminate: true

                        background: Rectangle {
                            radius: 6
                            color: "#10233d"
                            border.width: 1
                            border.color: "#2f5b84"
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#0f2139" }
                                GradientStop { position: 1.0; color: "#132a45" }
                            }
                        }

                        contentItem: Item {
                            id: progressContent
                            clip: true

                            Rectangle {
                                id: progressSweep
                                width: Math.max(90, progressContent.width * 0.35)
                                height: progressContent.height
                                radius: height * 0.5
                                x: -width
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#1f7bc700" }
                                    GradientStop { position: 0.55; color: "#4cc5ffdd" }
                                    GradientStop { position: 1.0; color: "#7ce6ffc0" }
                                }

                                SequentialAnimation on x {
                                    running: catalogProgressBar.visible
                                    loops: Animation.Infinite
                                    NumberAnimation {
                                        from: -progressSweep.width
                                        to: progressContent.width
                                        duration: 1050
                                        easing.type: Easing.InOutCubic
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
