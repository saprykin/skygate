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
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            padding: 8

            Button {
                id: liveButton
                text: skyContext.live ? "Live (On)" : "Live (Off)"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: skyContext.setLive(!skyContext.live)
            }
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
                id: modeLabel
                anchors.verticalCenter: parent.verticalCenter
                text: skyContext.live ? "Mode: Live" : "Mode: Paused"
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
        function onInvalidUtcDateInput(utcDateText) {
            utcDateInput.text = skyContext.utcDateText
        }
        function onInvalidUtcTimeInput(utcTimeText) {
            utcTimeInput.text = skyContext.utcTimeText
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
