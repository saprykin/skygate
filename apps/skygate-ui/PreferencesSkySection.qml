import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property alias utcDateInput: utcDateInput
    property alias utcTimeInput: utcTimeInput
    property alias latitudeInput: latitudeInput
    property alias longitudeInput: longitudeInput
    property alias elevationInput: elevationInput
    property alias projectionCombo: projectionCombo
    property alias utcDateTimeLockToggle: utcDateTimeLockToggle

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#0f1d39"
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
                model: ["Stereographic", "AzimuthalEquidistant", "Perspective"]
            }
        }
    }
}
