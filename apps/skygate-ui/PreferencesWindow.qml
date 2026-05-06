import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import com.skygate.app 1.0

Window {
    id: preferencesWindow
    objectName: "preferencesWindow"
    title: "Preferences"
    width: 588
    height: 430
    minimumWidth: 502
    minimumHeight: 400
    visible: false
    color: skyContext.theme.windowBackground
    required property var skyContextController
    property Window transientParentWindow
    transientParent: transientParentWindow
    flags: Qt.Dialog
    modality: Qt.WindowModal

    property color cardBackground: skyContext.theme.cardBackground
    property color cardBackgroundBottom: skyContext.theme.cardBackgroundBottom
    property color cardBorder: skyContext.theme.cardBorder
    property color dividerColor: skyContext.theme.dividerColor
    property color textPrimary: skyContext.theme.textPrimary
    property color textSecondary: skyContext.theme.textSecondary
    property color textMuted: skyContext.theme.textMuted
    property int selectedPage: 0
    readonly property bool catalogBusy: skyContextController !== null
                                        && (skyContextController.downloadingCatalog
                                            || skyContextController.catalogProcessing)
    readonly property bool applyEnabled: !(selectedPage === 1
                                           && preferencesDraft.locationSourceText === "City"
                                           && preferencesDraft.selectedCityId === "")
    readonly property string currentSectionDescription: {
        if (selectedPage === 0) {
            return "Application behavior and diagnostics"
        }
        if (selectedPage === 1) {
            return "Observer location and projection"
        }
        if (selectedPage === 2) {
            return "Theme and visual presentation"
        }
        return "Catalog source and download settings"
    }

    PreferencesDraft {
        id: preferencesDraft
        objectName: "preferencesDraft"
        skyContextController: preferencesWindow.skyContextController
    }

    Connections {
        target: preferencesWindow.skyContextController

        function onLatitudeTextChanged() {
            preferencesDraft.syncDeviceLocationFromContext()
        }

        function onLongitudeTextChanged() {
            preferencesDraft.syncDeviceLocationFromContext()
        }

        function onElevationTextChanged() {
            preferencesDraft.syncDeviceLocationFromContext()
        }

        function onLocationSourceTextChanged() {
            preferencesDraft.syncDeviceLocationFromContext()
        }
    }

    function syncFormFromContext() {
        preferencesDraft.resetFromContext()
    }

    function applyFormToContext() {
        preferencesDraft.applyToContext()
    }

    function openWindow() {
        syncFormFromContext()
        visible = true
        raise()
        requestActivate()
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
        radius: 16
        clip: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: preferencesWindow.cardBackground }
            GradientStop { position: 1.0; color: preferencesWindow.cardBackgroundBottom }
        }
        border.width: 1
        border.color: preferencesWindow.cardBorder

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 82
            color: "transparent"
            gradient: Gradient {
                GradientStop { position: 0.0; color: skyContext.theme.headerGradientStart }
                GradientStop { position: 1.0; color: skyContext.theme.headerGradientEnd }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 64

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 44
                    anchors.topMargin: 10
                    anchors.bottomMargin: 9
                    spacing: 1

                    Label {
                        text: "Preferences"
                        color: preferencesWindow.textPrimary
                        font.family: "Avenir Next"
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }

                    Label {
                        text: preferencesWindow.currentSectionDescription
                        color: preferencesWindow.textSecondary
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                    }
                }

                ToolButton {
                    id: closeIconButton
                    objectName: "preferencesCloseButton"
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 9
                    anchors.rightMargin: 9
                    width: 30
                    height: 30
                    text: "\u2715"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    onClicked: preferencesWindow.visible = false
                    ToolTip.visible: hovered
                    ToolTip.text: "Close"

                    contentItem: Text {
                        text: closeIconButton.text
                        color: skyContext.theme.closeButtonText
                        font: closeIconButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 15
                        color: closeIconButton.down
                            ? skyContext.theme.closeButtonBackgroundPressed
                            : (closeIconButton.hovered
                                   ? skyContext.theme.closeButtonBackgroundHover
                                   : skyContext.theme.closeButtonBackground)
                        border.width: 1
                        border.color: skyContext.theme.closeButtonBorder
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: preferencesWindow.dividerColor
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Item {
                    Layout.preferredWidth: 132
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.topMargin: 10
                        anchors.bottomMargin: 8
                        spacing: 6

                        Label {
                            text: "Sections"
                            color: skyContext.theme.sectionTitleText
                            font.family: "Avenir Next"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        PreferencesSectionButton {
                            objectName: "preferencesGeneralSectionButton"
                            label: "General"
                            active: preferencesWindow.selectedPage === 0
                            onClicked: preferencesWindow.selectedPage = 0
                        }

                        PreferencesSectionButton {
                            objectName: "preferencesSkySectionButton"
                            label: "Sky"
                            active: preferencesWindow.selectedPage === 1
                            onClicked: preferencesWindow.selectedPage = 1
                        }

                        PreferencesSectionButton {
                            objectName: "preferencesAppearanceSectionButton"
                            label: "Appearance"
                            active: preferencesWindow.selectedPage === 2
                            onClicked: preferencesWindow.selectedPage = 2
                        }

                        PreferencesSectionButton {
                            objectName: "preferencesCatalogSectionButton"
                            label: "Catalog"
                            active: preferencesWindow.selectedPage === 3
                            onClicked: preferencesWindow.selectedPage = 3
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: preferencesWindow.dividerColor
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        spacing: 4

                        StackLayout {
                            objectName: "preferencesSectionStack"
                            currentIndex: preferencesWindow.selectedPage
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            PreferencesGeneralSection {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesSkySection {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesAppearanceSection {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesCatalogSection {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                skyContextController: preferencesWindow.skyContextController
                                preferencesDraft: preferencesDraft
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: preferencesWindow.dividerColor
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 42

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 5
                    anchors.bottomMargin: 5
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        visible: preferencesWindow.selectedPage === 3 && preferencesWindow.catalogBusy

                        Label {
                            Layout.fillWidth: true
                            text: {
                                const statusText = preferencesWindow.skyContextController.catalogStatusText || ""
                                return statusText.startsWith("Catalog: ")
                                    ? statusText.substring(9)
                                    : statusText
                            }
                            color: preferencesWindow.textSecondary
                            font.family: "Avenir Next"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        ProgressBar {
                            id: footerCatalogProgressBar
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            from: 0
                            to: 1
                            value: 0
                            indeterminate: true

                            background: Rectangle {
                                radius: 3
                                color: skyContext.theme.progressBarTrack
                                border.width: 1
                                border.color: skyContext.theme.progressBarTrackBorder
                            }

                            contentItem: Item {
                                id: footerProgressContent
                                clip: true

                                Rectangle {
                                    id: footerProgressSweep
                                    width: Math.max(48, footerProgressContent.width * 0.24)
                                    height: footerProgressContent.height
                                    radius: height * 0.5
                                    x: -width
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: skyContext.theme.progressBarSweepStart }
                                        GradientStop { position: 0.5; color: skyContext.theme.progressBarSweepMid }
                                        GradientStop { position: 1.0; color: skyContext.theme.progressBarSweepEnd }
                                    }

                                    SequentialAnimation on x {
                                        running: footerCatalogProgressBar.visible
                                        loops: Animation.Infinite
                                        NumberAnimation {
                                            from: -footerProgressSweep.width
                                            to: footerProgressContent.width
                                            duration: 1050
                                            easing.type: Easing.InOutCubic
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        Layout.preferredWidth: preferencesWindow.selectedPage === 3
                                               && preferencesWindow.catalogBusy ? 0 : 1
                        Layout.fillWidth: !(preferencesWindow.selectedPage === 3
                                            && preferencesWindow.catalogBusy)
                    }

                    PreferencesActionButton {
                        objectName: "preferencesApplyButton"
                        Layout.preferredWidth: 116
                        Layout.preferredHeight: 32
                        primary: true
                        enabled: preferencesWindow.applyEnabled
                        text: "Apply"
                        onClicked: preferencesWindow.applyFormToContext()
                    }
                }
            }
        }
    }
}
