import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import com.skygate.app 1.0

Window {
    id: preferencesWindow
    title: "Preferences"
    width: 760
    height: 560
    minimumWidth: 700
    minimumHeight: 520
    visible: false
    required property var skyContextController
    property Window transientParentWindow
    transientParent: transientParentWindow
    flags: Qt.Dialog
    modality: Qt.WindowModal

    property color frameBackground: "#0d162c"
    property color panelBackground: "#122243"
    property color sectionBackground: "#0f1d39"
    property color borderColor: "#345984"
    property color textPrimary: "#ecf4ff"
    property color textSecondary: "#9fb8dd"
    property int selectedPage: 0


    function syncFormFromContext() {
        utcDateTimeLockToggle.checked =
            skyContextController.utcDateLocked || skyContextController.utcTimeLocked
        utcDateInput.text = skyContextController.utcDateText
        utcTimeInput.text = skyContextController.utcTimeText
        latitudeInput.text = skyContextController.latitudeText
        longitudeInput.text = skyContextController.longitudeText
        elevationInput.text = skyContextController.elevationText
        projectionCombo.currentIndex = Math.max(0, projectionCombo.model.indexOf(skyContextController.projectionTypeText))
        catalogSection.catalogPresetCurrentIndex = Math.max(
            0,
            Math.min(catalogSection.catalogPresetCount - 1, skyContextController.catalogPresetIndex())
        )
        catalogSection.catalogUrlText = skyContextController.catalogUrlText()
    }

    function applyFormToContext() {
        skyContextController.setUtcDateLocked(utcDateTimeLockToggle.checked)
        skyContextController.setUtcTimeLocked(utcDateTimeLockToggle.checked)
        skyContextController.setUtcDateText(utcDateInput.text)
        skyContextController.setUtcTimeText(utcTimeInput.text)
        skyContextController.setLatitudeText(latitudeInput.text)
        skyContextController.setLongitudeText(longitudeInput.text)
        skyContextController.setElevationText(elevationInput.text)
        skyContextController.setProjectionTypeText(projectionCombo.currentText)
        skyContextController.setCatalogPresetIndex(catalogSection.catalogPresetCurrentIndex)
        skyContextController.setCatalogUrlText(catalogSection.catalogUrlText)
        syncFormFromContext()
    }

    Shortcut {
        sequence: "Esc"
        onActivated: preferencesWindow.visible = false
    }

    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: preferencesWindow.visible = false
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#13284a" }
            GradientStop { position: 0.5; color: "#0d1f3a" }
            GradientStop { position: 1.0; color: "#091326" }
        }

        Rectangle {
            width: 320
            height: 320
            radius: 160
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: -120
            anchors.leftMargin: -90
            color: "#4ec8ef22"
        }

        Rectangle {
            width: 220
            height: 220
            radius: 110
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.bottomMargin: -80
            anchors.rightMargin: -50
            color: "#2f82c422"
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 710)
            height: Math.min(parent.height - 40, 530)
            radius: 22
            color: preferencesWindow.frameBackground
            border.width: 1
            border.color: preferencesWindow.borderColor
            opacity: preferencesWindow.visible ? 1.0 : 0.0
            scale: preferencesWindow.visible ? 1.0 : 0.985

            Behavior on opacity {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: "Preferences"
                            color: preferencesWindow.textPrimary
                            font.family: "Avenir Next"
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                        }

                        Label {
                            text: "Observer settings and catalog management"
                            color: preferencesWindow.textSecondary
                            font.family: "Avenir Next"
                        }
                    }

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 16
                    color: preferencesWindow.panelBackground
                    border.width: 1
                    border.color: preferencesWindow.borderColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 170
                            Layout.fillHeight: true
                            radius: 12
                            color: "#0e1d38"
                            border.width: 1
                            border.color: "#2f517a"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    text: "Sections"
                                    color: preferencesWindow.textSecondary
                                    font.family: "Avenir Next"
                                    font.weight: Font.DemiBold
                                }

                                Rectangle {
                                    id: skySectionButton
                                    Layout.fillWidth: true
                                    implicitHeight: 38
                                    radius: 10
                                    readonly property bool active: preferencesWindow.selectedPage === 0
                                    color: active
                                           ? "#2f79b8"
                                           : (skySectionMouse.pressed ? "#213c5b" : (skySectionMouse.containsMouse ? "#1c3452" : "#142943"))
                                    border.width: 1
                                    border.color: active ? "#9de2ff" : "#4f769e"

                                    Label {
                                        anchors.centerIn: parent
                                        text: "Sky"
                                        color: skySectionButton.active ? "#ecf8ff" : "#bdd4f4"
                                        font.family: "Avenir Next"
                                        font.weight: Font.DemiBold
                                    }

                                    MouseArea {
                                        id: skySectionMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: preferencesWindow.selectedPage = 0
                                    }
                                }

                                Rectangle {
                                    id: catalogSectionButton
                                    Layout.fillWidth: true
                                    implicitHeight: 38
                                    radius: 10
                                    readonly property bool active: preferencesWindow.selectedPage === 1
                                    color: active
                                           ? "#2f79b8"
                                           : (catalogSectionMouse.pressed ? "#213c5b" : (catalogSectionMouse.containsMouse ? "#1c3452" : "#142943"))
                                    border.width: 1
                                    border.color: active ? "#9de2ff" : "#4f769e"

                                    Label {
                                        anchors.centerIn: parent
                                        text: "Catalog"
                                        color: catalogSectionButton.active ? "#ecf8ff" : "#bdd4f4"
                                        font.family: "Avenir Next"
                                        font.weight: Font.DemiBold
                                    }

                                    MouseArea {
                                        id: catalogSectionMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: preferencesWindow.selectedPage = 1
                                    }
                                }

                                Item {
                                    Layout.fillHeight: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: preferencesWindow.selectedPage === 0
                                          ? "Observer location and projection"
                                          : "Catalog source and download settings"
                                    color: "#89a8d2"
                                    font.family: "Avenir Next"
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        StackLayout {
                            currentIndex: preferencesWindow.selectedPage
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Item {
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 12
                                    color: preferencesWindow.sectionBackground
                                    border.width: 1
                                    border.color: "#2b4b74"

                                    GridLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        columns: 3
                                        rowSpacing: 8
                                        columnSpacing: 12

                                        Label {
                                            text: "UTC Date"
                                            color: "#cad9f7"
                                            font.family: "Avenir Next"
                                        }
                                        PreferencesTextField {
                                            id: utcDateInput
                                            Layout.fillWidth: true
                                            placeholderText: "YYYY-MM-DD"
                                            enabled: !utcDateTimeLockToggle.checked
                                        }
                                        ToolButton {
                                            id: utcDateTimeLockToggle
                                            checkable: true
                                            Layout.rowSpan: 2
                                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                            width: 38
                                            height: 30
                                            text: checked ? "\uD83D\uDD12" : "\uD83D\uDD13"
                                            font.pixelSize: 16
                                            ToolTip.visible: hovered
                                            ToolTip.text: checked
                                                ? "Using current UTC date/time"
                                                : "Using manual UTC date/time input"

                                            contentItem: Text {
                                                text: parent.text
                                                color: "#eaf7ff"
                                                font: parent.font
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                radius: 8
                                                color: utcDateTimeLockToggle.checked
                                                    ? "#2f79b8"
                                                    : (utcDateTimeLockToggle.down
                                                        ? "#27476d"
                                                        : (utcDateTimeLockToggle.hovered ? "#315881" : "#1a3352"))
                                                border.width: 1
                                                border.color: utcDateTimeLockToggle.checked ? "#9de2ff" : "#6fbde6"
                                            }
                                        }

                                        Label {
                                            text: "UTC Time"
                                            color: "#cad9f7"
                                            font.family: "Avenir Next"
                                        }
                                        PreferencesTextField {
                                            id: utcTimeInput
                                            Layout.fillWidth: true
                                            placeholderText: "HH:MM:SS"
                                            enabled: !utcDateTimeLockToggle.checked
                                        }

                                        Label {
                                            text: "Latitude"
                                            color: "#cad9f7"
                                            font.family: "Avenir Next"
                                        }
                                        PreferencesTextField {
                                            id: latitudeInput
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            placeholderText: "-90..90"
                                            validator: DoubleValidator {
                                                bottom: -90.0
                                                top: 90.0
                                                notation: DoubleValidator.StandardNotation
                                            }
                                        }

                                        Label {
                                            text: "Longitude"
                                            color: "#cad9f7"
                                            font.family: "Avenir Next"
                                        }
                                        PreferencesTextField {
                                            id: longitudeInput
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            placeholderText: "-180..180"
                                            validator: DoubleValidator {
                                                bottom: -180.0
                                                top: 180.0
                                                notation: DoubleValidator.StandardNotation
                                            }
                                        }

                                        Label {
                                            text: "Elevation (m)"
                                            color: "#cad9f7"
                                            font.family: "Avenir Next"
                                        }
                                        PreferencesTextField {
                                            id: elevationInput
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            placeholderText: "meters"
                                            validator: DoubleValidator {
                                                notation: DoubleValidator.StandardNotation
                                            }
                                        }

                                        Label {
                                            text: "Projection"
                                            color: "#cad9f7"
                                            font.family: "Avenir Next"
                                        }
                                        PreferencesComboBox {
                                            id: projectionCombo
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            model: ["Stereographic", "Perspective"]
                                        }
                                    }
                                }
                            }

                            PreferencesCatalogSection {
                                id: catalogSection
                                skyContextController: preferencesWindow.skyContextController
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    Layout.minimumHeight: 52
                    implicitHeight: 52
                    radius: 12
                    color: "#0f1c35"
                    border.width: 1
                    border.color: "#2f5078"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Item {
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            id: applyButton
                            Layout.preferredWidth: 112
                            Layout.fillHeight: true
                            radius: 10
                            color: applyMouse.pressed ? "#296fa9" : (applyMouse.containsMouse ? "#378ac8" : "#307fbf")
                            border.width: 1
                            border.color: "#b9ecff"

                            Label {
                                anchors.centerIn: parent
                                text: "Apply"
                                color: "#f4fbff"
                                font.family: "Avenir Next"
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: applyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: preferencesWindow.applyFormToContext()
                            }
                        }
                    }
                }
            }

            ToolButton {
                id: closeIconButton
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 12
                anchors.rightMargin: 12
                width: 34
                height: 34
                text: "\u2715"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                onClicked: preferencesWindow.visible = false
                ToolTip.visible: hovered
                ToolTip.text: "Close"

                contentItem: Text {
                    text: closeIconButton.text
                    color: "#eaf7ff"
                    font: closeIconButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 17
                    color: closeIconButton.down ? "#27476d" : (closeIconButton.hovered ? "#315881" : "#1a3352")
                    border.width: 1
                    border.color: "#6fbde6"
                }
            }
        }
    }
}
