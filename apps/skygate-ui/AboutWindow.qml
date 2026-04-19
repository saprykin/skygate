import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: aboutWindow
    title: "About SkyGate"
    width: 420
    height: 320
    minimumWidth: 420
    minimumHeight: 320
    maximumWidth: 420
    maximumHeight: 320
    visible: false
    color: "#171b30"
    property Window transientParentWindow
    transientParent: transientParentWindow
    flags: Qt.Dialog
    modality: Qt.WindowModal
    readonly property int currentYear: (new Date()).getFullYear()

    Shortcut {
        sequence: "Esc"
        onActivated: aboutWindow.visible = false
    }

    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: aboutWindow.visible = false
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        clip: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1d2138" }
            GradientStop { position: 1.0; color: "#1a1e33" }
        }
        border.width: 1
        border.color: "#474d70"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 60
            color: "transparent"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#101631" }
                GradientStop { position: 1.0; color: "#00101631" }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 42

                ToolButton {
                    id: closeButton
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 8
                    anchors.rightMargin: 8
                    width: 30
                    height: 30
                    text: "\u2715"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    onClicked: aboutWindow.visible = false

                    contentItem: Text {
                        text: closeButton.text
                        color: "#edf1ff"
                        font: closeButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 15
                        color: closeButton.down
                            ? "#232843"
                            : (closeButton.hovered ? "#292f4c" : "#1f2339")
                        border.width: 1
                        border.color: "#596084"
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 2
                Layout.bottomMargin: 14
                spacing: 8

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    property var aboutLogoSources: [
                        "resources/icons/skygate-icon-512.png",
                        "qrc:/icons/app-icon-512.png",
                        "qrc:/qt/qml/com/skygate/app/resources/icons/skygate-icon-512.png"
                    ]
                    property int aboutLogoSourceIndex: 0

                    Rectangle {
                        anchors.fill: parent
                        radius: 14
                        color: "#232742"
                        border.width: 1
                        border.color: "#4c5278"
                    }

                    Image {
                        id: aboutLogoImage
                        anchors.fill: parent
                        anchors.margins: 10
                        source: parent.aboutLogoSources[parent.aboutLogoSourceIndex]
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: false
                        onStatusChanged: {
                            if (status === Image.Error
                                && parent.aboutLogoSourceIndex + 1 < parent.aboutLogoSources.length) {
                                parent.aboutLogoSourceIndex += 1
                                source = parent.aboutLogoSources[parent.aboutLogoSourceIndex]
                            }
                        }
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "SkyGate"
                    color: "#f2f4ff"
                    font.family: "Avenir Next"
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\u00A9 " + aboutWindow.currentYear + "  |  Version " + Qt.application.version
                    color: "#9ca3c5"
                    font.family: "Avenir Next"
                    font.pixelSize: 13
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.preferredHeight: 1
                    color: "#343955"
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: quoteColumn.implicitHeight + 20
                    radius: 8
                    color: "#232742"
                    border.width: 1
                    border.color: "#4c5278"

                    Column {
                        id: quoteColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Label {
                            width: parent.width
                            text: "\"Across the sea of space, the stars are other suns.\""
                            color: "#edf0fe"
                            font.family: "Avenir Next"
                            font.pixelSize: 14
                            font.italic: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }

                        Label {
                            width: parent.width
                            text: "- Carl Sagan"
                            color: "#878dad"
                            font.family: "Avenir Next"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                PreferencesActionButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 2
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 32
                    primary: true
                    text: "Close"
                    onClicked: aboutWindow.visible = false
                }
            }
        }
    }
}
