import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: aboutWindow
    title: "About SkyGate"
    width: 560
    height: 470
    minimumWidth: 520
    minimumHeight: 430
    visible: false
    property Window transientParentWindow
    transientParent: transientParentWindow
    flags: Qt.Dialog
    modality: Qt.WindowModal

    property color frameBackground: "#0d162c"
    property color panelBackground: "#122243"
    property color borderColor: "#345984"
    property color textPrimary: "#ecf4ff"
    property color textSecondary: "#9fb8dd"
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
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#13284a" }
            GradientStop { position: 0.5; color: "#0d1f3a" }
            GradientStop { position: 1.0; color: "#091326" }
        }

        Rectangle {
            width: 240
            height: 240
            radius: 120
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: -90
            anchors.leftMargin: -70
            color: "#4ec8ef22"
        }

        Rectangle {
            width: 180
            height: 180
            radius: 90
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.bottomMargin: -60
            anchors.rightMargin: -40
            color: "#2f82c422"
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 56, 500)
            height: Math.min(parent.height - 56, 410)
            radius: 24
            color: aboutWindow.frameBackground
            border.width: 1
            border.color: aboutWindow.borderColor
            opacity: aboutWindow.visible ? 1.0 : 0.0
            scale: aboutWindow.visible ? 1.0 : 0.985

            Behavior on opacity {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 12

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 132
                    property var aboutLogoSources: [
                        "resources/icons/skygate-icon-512.png",
                        "qrc:/icons/app-icon-512.png",
                        "qrc:/qt/qml/com/skygate/app/resources/icons/skygate-icon-512.png"
                    ]
                    property int aboutLogoSourceIndex: 0

                    Rectangle {
                        anchors.fill: parent
                        radius: 25
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#1e395f" }
                            GradientStop { position: 1.0; color: "#102544" }
                        }
                        border.width: 2
                        border.color: "#9edfff"
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 8
                        radius: 18
                        color: "#0a1a34"
                        border.width: 1
                        border.color: "#63a1d4"
                    }

                    Image {
                        id: aboutLogoImage
                        anchors.fill: parent
                        anchors.margins: 14
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
                    color: aboutWindow.textPrimary
                    font.family: "Avenir Next"
                    font.pixelSize: 58
                    font.weight: Font.DemiBold
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\u00A9 " + aboutWindow.currentYear
                    color: aboutWindow.textSecondary
                    font.family: "Avenir Next"
                    font.pixelSize: 20
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#3f638f"
                    opacity: 0.7
                }

                Label {
                    Layout.fillWidth: true
                    text: "\"Across the sea of space, the stars are other suns.\""
                    color: "#d8e9ff"
                    font.family: "Avenir Next"
                    font.pixelSize: 17
                    font.italic: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: "- Carl Sagan"
                    color: aboutWindow.textSecondary
                    font.family: "Avenir Next"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                }

                Item {
                    Layout.fillHeight: true
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Close"
                    font.family: "Avenir Next"
                    font.weight: Font.DemiBold
                    implicitWidth: 120
                    implicitHeight: 38
                    onClicked: aboutWindow.visible = false

                    contentItem: Text {
                        text: parent.text
                        color: "#f4fbff"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.down ? "#296fa9" : (parent.hovered ? "#378ac8" : "#307fbf")
                        border.width: 1
                        border.color: "#b9ecff"
                    }
                }
            }
        }
    }
}
