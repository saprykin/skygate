import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import com.skygate.app 1.0

ApplicationWindow {
    id: root
    width: 1100
    height: 760
    visible: true
    title: "Skygate"

    function syncSettingsFormFromContext() {
        liveCheckBox.checked = skyContext.live
        utcDateInput.text = skyContext.utcDateText
        utcTimeInput.text = skyContext.utcTimeText
        latitudeInput.text = skyContext.latitudeText
        longitudeInput.text = skyContext.longitudeText
        elevationInput.text = skyContext.elevationText
        projectionCombo.currentIndex = Math.max(0, projectionCombo.model.indexOf(skyContext.projectionTypeText))
    }

    function nearestIndex(values, target) {
        let nearest = 0
        let nearestDistance = Math.abs(values[0] - target)
        for (let i = 1; i < values.length; ++i) {
            const distance = Math.abs(values[i] - target)
            if (distance < nearestDistance) {
                nearest = i
                nearestDistance = distance
            }
        }
        return nearest
    }

    function applySettingsFormToContext() {
        skyContext.setUtcDateText(utcDateInput.text)
        skyContext.setUtcTimeText(utcTimeInput.text)
        skyContext.setLatitudeText(latitudeInput.text)
        skyContext.setLongitudeText(longitudeInput.text)
        skyContext.setElevationText(elevationInput.text)
        skyContext.setProjectionTypeText(projectionCombo.currentText)
        skyContext.setLive(liveCheckBox.checked)
        syncSettingsFormFromContext()
    }

    menuBar: MenuBar {
        Menu {
            title: "&App"
            MenuItem {
                text: "&Preferences..."
                onTriggered: {
                    root.syncSettingsFormFromContext()
                    preferencesWindow.visible = true
                    preferencesWindow.raise()
                    preferencesWindow.requestActivate()
                }
            }
        }
    }

    footer: Rectangle {
        height: 48
        color: "#0b1428"

        Row {
            id: statusLeftRow
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: statusTime.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 20

            Label {
                text: skyContext.live ? "Live: On" : "Live: Off"
                color: skyContext.live ? "#7fe39f" : "#f2b0b0"
            }
            Label {
                text: skyContext.locationStatusText
                color: "#a9c4ff"
            }
            Label {
                text: "Lat " + skyContext.latitudeText
                      + " | Lon " + skyContext.longitudeText
                      + " | Elev " + skyContext.elevationText + " m"
                      + " | Proj " + skyContext.projectionTypeText
                color: "#9ab0d6"
                elide: Text.ElideRight
                width: Math.max(120, statusLeftRow.width - 320)
            }
        }

        Label {
            id: statusTime
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 210
            horizontalAlignment: Text.AlignRight
            text: skyContext.utcDateText + " " + skyContext.utcTimeText + " UTC"
            color: "#d7e3ff"
            font.family: "monospace"
        }
    }

    Connections {
        target: skyContext
        function onSpeedMultiplierChanged() {
            speedCombo.currentIndex = root.nearestIndex(
                timelineToolbarRow.speedValues,
                skyContext.speedMultiplier
            )
        }
        function onStepSecondsChanged() {
            stepCombo.currentIndex = root.nearestIndex(
                timelineToolbarRow.stepValues,
                skyContext.stepSeconds
            )
        }
    }

    Component.onCompleted: {
        speedCombo.currentIndex = root.nearestIndex(
            timelineToolbarRow.speedValues,
            skyContext.speedMultiplier
        )
        stepCombo.currentIndex = root.nearestIndex(
            timelineToolbarRow.stepValues,
            skyContext.stepSeconds
        )
    }

    Window {
        id: preferencesWindow
        title: "Preferences"
        width: 680
        height: 520
        visible: false
        transientParent: root
        flags: Qt.Dialog

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#111a30" }
                GradientStop { position: 1.0; color: "#060d1b" }
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 620)
                height: Math.min(parent.height - 32, 460)
                radius: 20
                color: "#13203d"
                border.width: 1
                border.color: "#2e436f"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 16

                    Label {
                        text: "Sky Preferences"
                        color: "#e8f0ff"
                        font.family: "Avenir Next"
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }

                    Label {
                        text: "Observer, time, and projection settings"
                        color: "#9bb1d9"
                        font.family: "Avenir Next"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 14
                        color: "#0e1830"
                        border.width: 1
                        border.color: "#243b67"

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 12

                            Label {
                                text: "Live updates"
                                color: "#cad9f7"
                                font.family: "Avenir Next"
                            }
                            CheckBox {
                                id: liveCheckBox
                            }

                            Label {
                                text: "UTC Date"
                                color: "#cad9f7"
                                font.family: "Avenir Next"
                            }
                            TextField {
                                id: utcDateInput
                                Layout.fillWidth: true
                                placeholderText: "YYYY-MM-DD"
                            }

                            Label {
                                text: "UTC Time"
                                color: "#cad9f7"
                                font.family: "Avenir Next"
                            }
                            TextField {
                                id: utcTimeInput
                                Layout.fillWidth: true
                                placeholderText: "HH:MM:SS"
                            }

                            Label {
                                text: "Latitude"
                                color: "#cad9f7"
                                font.family: "Avenir Next"
                            }
                            TextField {
                                id: latitudeInput
                                Layout.fillWidth: true
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
                            TextField {
                                id: longitudeInput
                                Layout.fillWidth: true
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
                            TextField {
                                id: elevationInput
                                Layout.fillWidth: true
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
                            ComboBox {
                                id: projectionCombo
                                Layout.fillWidth: true
                                model: ["Stereographic", "AzimuthalEquidistant"]
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "Load Saved"
                            onClicked: {
                                skyContext.loadSettings()
                                root.syncSettingsFormFromContext()
                            }
                        }
                        Button {
                            text: "Save Current"
                            onClicked: {
                                root.applySettingsFormToContext()
                                skyContext.saveSettings()
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Close"
                            onClicked: preferencesWindow.visible = false
                        }
                        Button {
                            text: "Apply"
                            highlighted: true
                            onClicked: root.applySettingsFormToContext()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#081022" }
            GradientStop { position: 1.0; color: "#040912" }
        }

        SkyViewportItem {
            id: skyViewport
            anchors.fill: parent
            skyContextController: skyContext
        }

        Rectangle {
            id: summaryBadge
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 14
            width: summaryLabel.implicitWidth + 16
            height: summaryLabel.implicitHeight + 16
            radius: 10
            color: "#7f0b1428"
            border.width: 1
            border.color: "#335177"

            Label {
                id: summaryLabel
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: skyContext.skyContextSummary
                color: "#cbd9f6"
                font.family: "Avenir Next"
            }
        }

        Rectangle {
            id: timelineToolbar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            width: timelineToolbarRow.implicitWidth + 16
            height: timelineToolbarRow.implicitHeight + 14
            radius: 10
            color: "#7f0b1428"
            border.width: 1
            border.color: "#335177"

            Row {
                id: timelineToolbarRow
                anchors.centerIn: parent
                spacing: 6

                property var speedValues: [0.25, 0.5, 1.0, 2.0, 4.0, 8.0]
                property var stepValues: [1, 10, 60, 300, 3600]

                Label {
                    text: "Timeline"
                    color: "#cad9f7"
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: "Avenir Next"
                }

                Button {
                    text: skyContext.live ? "Pause" : "Play"
                    onClicked: skyContext.togglePlayPause()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: skyContext.live
                        ? "Pause live timeline updates"
                        : "Resume live timeline updates"
                }
                Button {
                    text: "<"
                    onClicked: skyContext.stepBackward()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Step backward by selected interval"
                }
                Button {
                    text: ">"
                    onClicked: skyContext.stepForward()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Step forward by selected interval"
                }
                ComboBox {
                    id: speedCombo
                    model: ["0.25x", "0.5x", "1x", "2x", "4x", "8x"]
                    implicitWidth: 78
                    onActivated: skyContext.setSpeedMultiplier(timelineToolbarRow.speedValues[currentIndex])
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Set live timeline speed multiplier"
                }
                ComboBox {
                    id: stepCombo
                    model: ["1s", "10s", "1m", "5m", "1h"]
                    implicitWidth: 74
                    onActivated: skyContext.setStepSeconds(timelineToolbarRow.stepValues[currentIndex])
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Set manual step interval"
                }
            }
        }
    }
}
