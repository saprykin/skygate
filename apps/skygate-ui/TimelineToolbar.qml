import QtQuick
import QtQuick.Controls

Item {
    id: toolbarRoot
    required property var skyContextController
    readonly property var theme: skyContextController.theme
    property var onRequestExpand: null
    property alias panelItem: timelineToolbarPanel
    property alias toggleItem: timelineToolbarToggle
    readonly property real expandedTotalWidth: timelineToolbarToggle.implicitWidth
        + timelineToolbarPanel.expandedWidth + 6

    readonly property var speedValues: [0.25, 0.5, 1.0, 2.0, 4.0, 8.0]
    readonly property var stepValues: [1, 10, 60, 300, 3600]
    readonly property var magnitudeValues: [2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
    z: 20

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

    function syncFromContext() {
        speedCombo.currentIndex = nearestIndex(speedValues, skyContextController.speedMultiplier)
        stepCombo.currentIndex = nearestIndex(stepValues, skyContextController.stepSeconds)
        magnitudeCombo.currentIndex = nearestIndex(
            magnitudeValues,
            skyContextController.magnitudeCutoff
        )
    }

    implicitWidth: skyContextController.timelineToolbarCollapsed
        ? timelineToolbarToggle.implicitWidth
        : timelineToolbarToggle.implicitWidth + timelineToolbarPanel.expandedWidth + 6
    implicitHeight: Math.max(
        timelineToolbarPanel.implicitHeight,
        timelineToolbarToggle.implicitHeight
    )

    Connections {
        target: skyContextController

        function onSpeedMultiplierChanged() {
            toolbarRoot.syncFromContext()
        }

        function onStepSecondsChanged() {
            toolbarRoot.syncFromContext()
        }

        function onMagnitudeCutoffChanged() {
            toolbarRoot.syncFromContext()
        }
    }

    Component.onCompleted: syncFromContext()

    Rectangle {
        id: timelineToolbarPanel
        anchors.top: parent.top
        anchors.right: timelineToolbarToggle.left
        anchors.rightMargin: 6
        property bool collapsed: skyContextController.timelineToolbarCollapsed
        readonly property real expandedWidth: timelineToolbarRow.implicitWidth + 16
        width: collapsed ? 0 : expandedWidth
        height: timelineToolbarRow.implicitHeight + 14
        implicitHeight: height
        radius: 10
        color: theme.toolbarPanelBackground
        border.width: 1
        border.color: theme.toolbarPanelBorder
        opacity: collapsed ? 0.0 : 1.0
        clip: true

        Behavior on width {
            NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Row {
            id: timelineToolbarRow
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            visible: !timelineToolbarPanel.collapsed
            enabled: !timelineToolbarPanel.collapsed

            component TimelineToolbarButton : Button {
                font.family: "Avenir Next"
                font.weight: Font.DemiBold
                implicitHeight: 38
                implicitWidth: Math.max(72, contentItem.implicitWidth + leftPadding + rightPadding)
                leftPadding: 16
                rightPadding: 16
                topPadding: 6
                bottomPadding: 6

                contentItem: Text {
                    text: parent.text
                    color: theme.toolbarButtonText
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: parent.down
                        ? theme.toolbarButtonBackgroundPressed
                        : (parent.hovered
                               ? theme.toolbarButtonBackgroundHover
                               : theme.toolbarButtonBackground)
                    border.width: 1
                    border.color: theme.toolbarButtonBorder
                }
            }

            component TimelineToolbarCombo : ComboBox {
                id: timelineComboControl
                font.family: "Avenir Next"
                font.weight: Font.DemiBold
                implicitHeight: 38
                leftPadding: 12
                rightPadding: 28
                topPadding: 5
                bottomPadding: 5

                contentItem: Text {
                    text: timelineComboControl.displayText
                    color: theme.toolbarPrimaryText
                    font: timelineComboControl.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                indicator: Text {
                    text: "\u25BE"
                    color: theme.inputIndicator
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                }

                background: Rectangle {
                    radius: 8
                    color: timelineComboControl.pressed
                        ? theme.toolbarButtonBackgroundPressed
                        : (timelineComboControl.hovered
                               ? theme.toolbarButtonBackgroundHover
                               : theme.toolbarButtonBackground)
                    border.width: 1
                    border.color: theme.toolbarButtonBorder
                }

                popup: Popup {
                    y: timelineComboControl.height + 4
                    width: timelineComboControl.width
                    padding: 4
                    implicitHeight: Math.min(contentItem.implicitHeight + 8, 220)

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: timelineComboControl.delegateModel
                        currentIndex: timelineComboControl.highlightedIndex
                    }

                    background: Rectangle {
                        radius: 10
                        color: theme.toolbarDropdownBackground
                        border.width: 1
                        border.color: theme.toolbarDropdownBorder
                    }
                }

                delegate: ItemDelegate {
                    id: timelineComboDelegate
                    width: timelineComboControl.width - 8
                    highlighted: timelineComboControl.highlightedIndex === index

                    contentItem: Text {
                        text: modelData
                        color: highlighted ? theme.toolbarPrimaryText : theme.toolbarSecondaryText
                        font: timelineComboControl.font
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: 7
                        color: highlighted
                            ? theme.toolbarItemBackgroundSelected
                            : (timelineComboDelegate.hovered
                                   ? theme.toolbarItemBackgroundHover
                                   : "transparent")
                    }
                }
            }

            TimelineToolbarButton {
                text: skyContextController.live ? "Pause" : "Play"
                onClicked: skyContextController.togglePlayPause()
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: skyContextController.live
                    ? "Pause live timeline updates"
                    : "Resume timeline playback from the current position"
            }

            TimelineToolbarButton {
                text: "<"
                onClicked: skyContextController.stepBackward()
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: "Step backward by selected interval"
            }

            TimelineToolbarButton {
                text: ">"
                onClicked: skyContextController.stepForward()
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: "Step forward by selected interval"
            }

            TimelineToolbarCombo {
                id: speedCombo
                model: ["0.25x", "0.5x", "1x", "2x", "4x", "8x"]
                implicitWidth: 78
                onActivated: skyContextController.setSpeedMultiplier(toolbarRoot.speedValues[currentIndex])
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: "Set catch-up playback speed multiplier"
            }

            TimelineToolbarCombo {
                id: stepCombo
                model: ["1s", "10s", "1m", "5m", "1h"]
                implicitWidth: 74
                onActivated: skyContextController.setStepSeconds(toolbarRoot.stepValues[currentIndex])
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: "Set manual step interval"
            }

            TimelineToolbarButton {
                text: "Reset"
                onClicked: skyContextController.resetViewDirection()
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: "Reset view direction to default south-up framing"
            }

            TimelineToolbarCombo {
                id: magnitudeCombo
                model: ["2.0", "3.0", "4.0", "5.0", "6.0", "7.0", "8.0"]
                implicitWidth: 70
                onActivated: skyContextController.setMagnitudeCutoff(
                    toolbarRoot.magnitudeValues[currentIndex]
                )
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: "Set star visual magnitude limit (higher value shows more stars)"
            }
        }
    }

    ToolButton {
        id: timelineToolbarToggle
        anchors.verticalCenter: timelineToolbarPanel.verticalCenter
        anchors.right: parent.right
        width: 30
        height: 44
        implicitWidth: width
        implicitHeight: height
        text: timelineToolbarPanel.collapsed ? "\u25B6" : "\u25C0"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        z: 11
        onClicked: {
            if (skyContextController.timelineToolbarCollapsed && toolbarRoot.onRequestExpand) {
                toolbarRoot.onRequestExpand()
            }

            skyContextController.setTimelineToolbarCollapsed(
                !skyContextController.timelineToolbarCollapsed
            )
        }
        ToolTip.visible: hovered
        ToolTip.delay: 250
        ToolTip.text: timelineToolbarPanel.collapsed
            ? "Show timeline panel"
            : "Hide timeline panel"

        contentItem: Text {
            text: timelineToolbarToggle.text
            color: theme.toolbarToggleText
            font: timelineToolbarToggle.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 8
            color: timelineToolbarToggle.down
                ? theme.toolbarToggleBackgroundPressed
                : (timelineToolbarToggle.hovered
                       ? theme.toolbarToggleBackgroundHover
                       : theme.toolbarToggleBackground)
            border.width: 1
            border.color: theme.toolbarToggleBorder
        }
    }
}
