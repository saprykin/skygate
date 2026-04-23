import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: aboutWindow
    title: "About SkyGate"
    width: 420
    height: 388
    minimumWidth: 420
    minimumHeight: 388
    maximumWidth: 420
    maximumHeight: 388
    visible: false
    readonly property var theme: skyContext.theme
    color: theme.windowBackground
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
            GradientStop { position: 0.0; color: theme.cardBackground }
            GradientStop { position: 1.0; color: theme.cardBackgroundBottom }
        }
        border.width: 1
        border.color: theme.cardBorder

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 60
            color: "transparent"
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.headerGradientStart }
                GradientStop { position: 1.0; color: theme.headerGradientEnd }
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
                        color: theme.closeButtonText
                        font: closeButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 15
                        color: closeButton.down
                            ? theme.closeButtonBackgroundPressed
                            : (closeButton.hovered
                                   ? theme.closeButtonBackgroundHover
                                   : theme.closeButtonBackground)
                        border.width: 1
                        border.color: theme.closeButtonBorder
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
                    Layout.preferredWidth: 76
                    Layout.preferredHeight: 76
                    clip: true
                    property var aboutLogoSources: [
                        "qrc:/icons/app-icon-512.png",
                        "qrc:/qt/qml/com/skygate/app/resources/icons/skygate-icon-512.png",
                        "resources/icons/skygate-icon-512.png"
                    ]
                    property int aboutLogoSourceIndex: 0
                    readonly property rect aboutLogoVisibleRect: Qt.rect(42, 37, 427, 437)
                    readonly property real aboutLogoRenderSize: Math.ceil(
                        (width * 512) / aboutLogoVisibleRect.height
                    )
                    readonly property real aboutLogoVisibleWidth: (
                        aboutLogoRenderSize * aboutLogoVisibleRect.width
                    ) / 512
                    readonly property real aboutLogoVisibleHeight: (
                        aboutLogoRenderSize * aboutLogoVisibleRect.height
                    ) / 512

                    Rectangle {
                        anchors.fill: parent
                        radius: 18
                        color: theme.logoPanelBackground
                        opacity: 0.62
                        border.width: 1
                        border.color: theme.logoPanelBorder
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: 17
                        color: "transparent"
                        border.width: 1
                        border.color: theme.logoPanelAccentBorder
                        opacity: 0.72
                    }

                    Image {
                        id: aboutLogoImage
                        width: parent.aboutLogoRenderSize
                        height: parent.aboutLogoRenderSize
                        x: Math.round(
                            ((parent.width - parent.aboutLogoVisibleWidth) * 0.5)
                            - ((parent.aboutLogoRenderSize * parent.aboutLogoVisibleRect.x) / 512)
                        )
                        y: Math.round(
                            ((parent.height - parent.aboutLogoVisibleHeight) * 0.5)
                            - ((parent.aboutLogoRenderSize * parent.aboutLogoVisibleRect.y) / 512)
                        )
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
                    color: theme.textPrimary
                    font.family: "Avenir Next"
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "\u00A9 " + aboutWindow.currentYear + "  |  Version " + Qt.application.version
                    color: theme.textSecondary
                    font.family: "Avenir Next"
                    font.pixelSize: 13
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: "Git " + skygateGitHash + "  |  Built " + skygateBuildDateTime
                    color: theme.monospaceText
                    font.family: "Menlo"
                    font.pixelSize: 11
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: "Messier and OpenNGC data: OpenNGC, CC-BY-SA-4.0"
                    color: theme.textMuted
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.preferredHeight: 1
                    color: theme.dividerColor
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: quoteColumn.implicitHeight + 20
                    radius: 8
                    color: theme.quoteBackground
                    border.width: 1
                    border.color: theme.quoteBorder

                    Column {
                        id: quoteColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Label {
                            width: parent.width
                            text: "\"Across the sea of space, the stars are other suns.\""
                            color: theme.quoteText
                            font.family: "Avenir Next"
                            font.pixelSize: 14
                            font.italic: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }

                        Label {
                            width: parent.width
                            text: "- Carl Sagan"
                            color: theme.textMuted
                            font.family: "Avenir Next"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                PreferencesActionButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    Layout.bottomMargin: 8
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
