import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    width: 1100
    height: 760
    visible: true
    title: "Skygate"

    header: ToolBar {
        Row {
            id: toolbarRow
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Label {
                id: utcDateLabel
                text: "UTC Date"
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: utcDateInput
                width: 120
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "YYYY-MM-DD"
                text: skyContext.utcDateText
                onEditingFinished: skyContext.setUtcDateText(text)
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        text = skyContext.utcDateText
                    }
                }
            }
            Label {
                id: utcTimeLabel
                text: "UTC Time"
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: utcTimeInput
                width: 96
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "HH:MM:SS"
                text: skyContext.utcTimeText
                onEditingFinished: skyContext.setUtcTimeText(text)
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        text = skyContext.utcTimeText
                    }
                }
            }
            Label {
                id: latitudeLabel
                text: "Lat"
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: latitudeInput
                width: 96
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "-90..90"
                text: skyContext.latitudeText
                validator: DoubleValidator {
                    bottom: -90.0
                    top: 90.0
                    notation: DoubleValidator.StandardNotation
                }
                onEditingFinished: skyContext.setLatitudeText(text)
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        text = skyContext.latitudeText
                    }
                }
            }
            Label {
                id: longitudeLabel
                text: "Lon"
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: longitudeInput
                width: 96
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "-180..180"
                text: skyContext.longitudeText
                validator: DoubleValidator {
                    bottom: -180.0
                    top: 180.0
                    notation: DoubleValidator.StandardNotation
                }
                onEditingFinished: skyContext.setLongitudeText(text)
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        text = skyContext.longitudeText
                    }
                }
            }
            Label {
                id: elevationLabel
                text: "Elev (m)"
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: elevationInput
                width: 96
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "meters"
                text: skyContext.elevationText
                validator: DoubleValidator {
                    notation: DoubleValidator.StandardNotation
                }
                onEditingFinished: skyContext.setElevationText(text)
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        text = skyContext.elevationText
                    }
                }
            }
        }

        Button {
            id: liveButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: skyContext.live ? "Live (On)" : "Live (Off)"
            onClicked: skyContext.setLive(!skyContext.live)
        }
    }

    footer: Rectangle {
        height: 40
        color: "#0b1428"

        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 20

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: skyContext.live ? "Live: On" : "Live: Off"
                color: skyContext.live ? "#7fe39f" : "#f2b0b0"
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: skyContext.locationStatusText
                color: "#a9c4ff"
            }
        }
    }

    Connections {
        target: skyContext
        function onUtcDateTextChanged() {
            if (!utcDateInput.activeFocus) {
                utcDateInput.text = skyContext.utcDateText
            }
        }
        function onUtcTimeTextChanged() {
            if (!utcTimeInput.activeFocus) {
                utcTimeInput.text = skyContext.utcTimeText
            }
        }
        function onLatitudeTextChanged() {
            if (!latitudeInput.activeFocus) {
                latitudeInput.text = skyContext.latitudeText
            }
        }
        function onLongitudeTextChanged() {
            if (!longitudeInput.activeFocus) {
                longitudeInput.text = skyContext.longitudeText
            }
        }
        function onElevationTextChanged() {
            if (!elevationInput.activeFocus) {
                elevationInput.text = skyContext.elevationText
            }
        }
        function onInvalidUtcDateInput(utcDateText) {
            utcDateInput.text = skyContext.utcDateText
        }
        function onInvalidUtcTimeInput(utcTimeText) {
            utcTimeInput.text = skyContext.utcTimeText
        }
        function onInvalidLatitudeInput(latitudeText) {
            latitudeInput.text = skyContext.latitudeText
        }
        function onInvalidLongitudeInput(longitudeText) {
            longitudeInput.text = skyContext.longitudeText
        }
        function onInvalidElevationInput(elevationText) {
            elevationInput.text = skyContext.elevationText
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#060c1a"

        Text {
            anchors.centerIn: parent
            text: "Sky rendering placeholder"
            color: "#d7e3ff"
            font.pixelSize: 22
        }
    }
}
