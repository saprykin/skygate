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
    color: theme.windowBackground
    required property var skyContextController
    readonly property var theme: skyContextController.theme
    property Window transientParentWindow
    readonly property bool useAttachedDialog: Qt.platform.os !== "linux"
    transientParent: useAttachedDialog ? transientParentWindow : null
    flags: (useAttachedDialog ? Qt.Dialog : Qt.Window) | Qt.FramelessWindowHint
    modality: useAttachedDialog ? Qt.WindowModal : Qt.NonModal

    property color cardBackground: theme.cardBackground
    property color cardBackgroundBottom: theme.cardBackgroundBottom
    property color cardBorder: theme.cardBorder
    property color dividerColor: theme.dividerColor
    property color textPrimary: theme.textPrimary
    property color textSecondary: theme.textSecondary
    property color textMuted: theme.textMuted
    // Section identities follow the preferencesSectionStack child order.
    readonly property int generalSection: 0
    readonly property int skySection: 1
    readonly property int engineSection: 2
    readonly property int appearanceSection: 3
    readonly property int catalogSection: 4
    property int selectedPage: generalSection
    readonly property bool catalogBusy: skyContextController !== null
                                        && (skyContextController.downloadingCatalog
                                            || skyContextController.catalogProcessing)
    readonly property bool ephemerisBusy: skyContextController !== null
                                           && skyContextController.ephemerisDataUpdateInProgress
    readonly property bool footerBusy: (selectedPage === engineSection && ephemerisBusy)
                                       || (selectedPage === catalogSection && catalogBusy)
    readonly property bool footerEngineStatusVisible: selectedPage === engineSection
        && skyContextController !== null
        && skyContextController.ephemerisDataStatusText !== ""
    readonly property bool footerCancelable: (selectedPage === engineSection && ephemerisBusy)
                                             || (selectedPage === catalogSection
                                                 && skyContextController !== null
                                                 && skyContextController.downloadingCatalog)
    readonly property bool applyEnabled: !(selectedPage === skySection
                                           && preferencesDraft.locationSourceText === "City"
                                           && preferencesDraft.selectedCityId === "")
    readonly property string currentSectionDescription: {
        if (selectedPage === generalSection) {
            return "Application behavior and diagnostics"
        }
        if (selectedPage === skySection) {
            return "Observer location and projection"
        }
        if (selectedPage === engineSection) {
            return "Ephemeris engine and data settings"
        }
        if (selectedPage === appearanceSection) {
            return "Theme and visual presentation"
        }
        return "Star and deep-sky catalog management"
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
                GradientStop { position: 0.0; color: theme.headerGradientStart }
                GradientStop { position: 1.0; color: theme.headerGradientEnd }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 64

                DragHandler {
                    target: null
                    acceptedButtons: Qt.LeftButton
                    onActiveChanged: if (active) preferencesWindow.startSystemMove()
                }

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
                        color: theme.closeButtonText
                        font: closeIconButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 15
                        color: closeIconButton.down
                            ? theme.closeButtonBackgroundPressed
                            : (closeIconButton.hovered
                                   ? theme.closeButtonBackgroundHover
                                   : theme.closeButtonBackground)
                        border.width: 1
                        border.color: theme.closeButtonBorder
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
                            color: theme.sectionTitleText
                            font.family: "Avenir Next"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        PreferencesSectionButton {
                            theme: preferencesWindow.theme
                            objectName: "preferencesGeneralSectionButton"
                            label: "General"
                            active: preferencesWindow.selectedPage === preferencesWindow.generalSection
                            onClicked: preferencesWindow.selectedPage = preferencesWindow.generalSection
                        }

                        PreferencesSectionButton {
                            theme: preferencesWindow.theme
                            objectName: "preferencesSkySectionButton"
                            label: "Sky"
                            active: preferencesWindow.selectedPage === preferencesWindow.skySection
                            onClicked: preferencesWindow.selectedPage = preferencesWindow.skySection
                        }

                        PreferencesSectionButton {
                            theme: preferencesWindow.theme
                            objectName: "preferencesEngineSectionButton"
                            label: "Engine"
                            active: preferencesWindow.selectedPage === preferencesWindow.engineSection
                            onClicked: preferencesWindow.selectedPage = preferencesWindow.engineSection
                        }

                        PreferencesSectionButton {
                            theme: preferencesWindow.theme
                            objectName: "preferencesAppearanceSectionButton"
                            label: "Appearance"
                            active: preferencesWindow.selectedPage === preferencesWindow.appearanceSection
                            onClicked: preferencesWindow.selectedPage = preferencesWindow.appearanceSection
                        }

                        PreferencesSectionButton {
                            theme: preferencesWindow.theme
                            objectName: "preferencesCatalogSectionButton"
                            label: "Catalog"
                            active: preferencesWindow.selectedPage === preferencesWindow.catalogSection
                            onClicked: preferencesWindow.selectedPage = preferencesWindow.catalogSection
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
                                theme: preferencesWindow.theme
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesSkySection {
                                theme: preferencesWindow.theme
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesEngineSection {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                skyContextController: preferencesWindow.skyContextController
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesAppearanceSection {
                                theme: preferencesWindow.theme
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                preferencesDraft: preferencesDraft
                            }

                            PreferencesCatalogSection {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                skyContextController: preferencesWindow.skyContextController
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
                        visible: preferencesWindow.footerBusy || preferencesWindow.footerEngineStatusVisible

                        Label {
                            Layout.fillWidth: true
                            objectName: "preferencesFooterStatusLabel"
                            visible: preferencesWindow.footerBusy || preferencesWindow.footerEngineStatusVisible
                            text: {
                                const statusText = preferencesWindow.selectedPage === preferencesWindow.engineSection
                                    ? (preferencesWindow.skyContextController.ephemerisDataStatusText || "")
                                    : (preferencesWindow.skyContextController.catalogStatusText || "")
                                return statusText.startsWith("Catalog: ")
                                    ? statusText.substring(9)
                                    : (statusText.startsWith("Ephemeris data: ")
                                       ? statusText.substring(16)
                                       : statusText)
                            }
                            color: preferencesWindow.textSecondary
                            font.family: "Avenir Next"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: preferencesWindow.footerBusy

                            ProgressBar {
                                id: footerCatalogProgressBar
                                objectName: "preferencesFooterProgressBar"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 6
                                Layout.alignment: Qt.AlignVCenter
                                from: 0
                                to: 1
                                value: preferencesWindow.ephemerisBusy
                                       ? preferencesWindow.skyContextController.ephemerisDataUpdateProgress
                                       : 0
                                indeterminate: !preferencesWindow.ephemerisBusy

                                background: Rectangle {
                                    radius: 3
                                    color: theme.progressBarTrack
                                    border.width: 1
                                    border.color: theme.progressBarTrackBorder
                                }

                                contentItem: Item {
                                    id: footerProgressContent
                                    clip: true

                                    Rectangle {
                                        visible: !footerCatalogProgressBar.indeterminate
                                        width: footerProgressContent.width * footerCatalogProgressBar.visualPosition
                                        height: footerProgressContent.height
                                        radius: height * 0.5
                                        color: theme.progressBarSweepMid
                                    }

                                    Rectangle {
                                        id: footerProgressSweep
                                        visible: footerCatalogProgressBar.indeterminate
                                        width: Math.max(48, footerProgressContent.width * 0.24)
                                        height: footerProgressContent.height
                                        radius: height * 0.5
                                        x: -width
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: theme.progressBarSweepStart }
                                            GradientStop { position: 0.5; color: theme.progressBarSweepMid }
                                            GradientStop { position: 1.0; color: theme.progressBarSweepEnd }
                                        }

                                        SequentialAnimation on x {
                                            running: footerCatalogProgressBar.visible
                                                     && footerCatalogProgressBar.indeterminate
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

                            Rectangle {
                                id: footerCancelButton
                                objectName: "preferencesFooterCancelDownloadButton"
                                visible: preferencesWindow.footerCancelable
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                Layout.alignment: Qt.AlignVCenter
                                radius: width * 0.5
                                color: footerCancelMouse.pressed
                                       ? theme.actionButtonSecondaryBottomPressed
                                       : (footerCancelMouse.containsMouse
                                          ? theme.actionButtonSecondaryTopHover
                                          : theme.actionButtonSecondaryTop)
                                border.width: 1
                                border.color: theme.actionButtonSecondaryBorder
                                ToolTip.visible: footerCancelMouse.containsMouse
                                ToolTip.text: "Cancel download"
                                ToolTip.delay: 450

                                Label {
                                    anchors.centerIn: parent
                                    text: "x"
                                    color: theme.actionButtonText
                                    font.family: "Avenir Next"
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    id: footerCancelMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: preferencesWindow.skyContextController.cancelActiveDownload()
                                }
                            }
                        }
                    }

                    Item {
                        Layout.preferredWidth: preferencesWindow.footerBusy ? 0 : 1
                        Layout.fillWidth: !preferencesWindow.footerBusy
                    }

                    PreferencesActionButton {
                        theme: preferencesWindow.theme
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
