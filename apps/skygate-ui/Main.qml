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
    title: "SkyGate"

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

    function overlapsTimelinePanel(itemX, itemY, itemWidth, itemHeight) {
        if (timelineToolbar.width <= 1 || timelineToolbar.opacity <= 0.05) {
            return false
        }

        const margin = 4
        const panelLeft = timelineToolbar.x - margin
        const panelTop = timelineToolbar.y - margin
        const panelRight = timelineToolbar.x + timelineToolbar.width + margin
        const panelBottom = timelineToolbar.y + timelineToolbar.height + margin

        const itemLeft = itemX
        const itemTop = itemY
        const itemRight = itemX + itemWidth
        const itemBottom = itemY + itemHeight
        return itemLeft < panelRight
            && itemRight > panelLeft
            && itemTop < panelBottom
            && itemBottom > panelTop
    }

    menuBar: MenuBar {
        Menu {
            title: "&App"
            MenuItem {
                text: "&About SkyGate"
                onTriggered: {
                    aboutWindow.visible = true
                    aboutWindow.raise()
                    aboutWindow.requestActivate()
                }
            }
            MenuSeparator {}
            MenuItem {
                text: "&Preferences..."
                onTriggered: {
                    preferencesWindow.syncFormFromContext()
                    preferencesWindow.visible = true
                    preferencesWindow.raise()
                    preferencesWindow.requestActivate()
                }
            }
        }
    }

    AboutWindow {
        id: aboutWindow
        transientParentWindow: root
    }

    PreferencesWindow {
        id: preferencesWindow
        skyContextController: skyContext
        transientParentWindow: root
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
                      + " | View Alt " + Number(skyContext.viewCenterAltitudeDeg).toFixed(1)
                      + " Az " + Number(skyContext.viewCenterAzimuthDeg).toFixed(1)
                      + " | Mag ≤ " + Number(skyContext.magnitudeCutoff).toFixed(1)
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
            font.family: "Menlo"
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
        function onMagnitudeCutoffChanged() {
            magnitudeCombo.currentIndex = root.nearestIndex(
                timelineToolbarRow.magnitudeValues,
                skyContext.magnitudeCutoff
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
        magnitudeCombo.currentIndex = root.nearestIndex(
            timelineToolbarRow.magnitudeValues,
            skyContext.magnitudeCutoff
        )
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

        MouseArea {
            id: viewPanArea
            anchors.fill: skyViewport
            hoverEnabled: true
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

            property real lastX: 0
            property real lastY: 0
            property real hoverX: 0
            property real hoverY: 0
            property string hoveredObjectLabel: ""
            readonly property real azimuthSensitivity: 0.18
            readonly property real altitudeSensitivity: 0.18

            onPressed: function(mouse) {
                lastX = mouse.x
                lastY = mouse.y
            }
            onPositionChanged: function(mouse) {
                hoverX = mouse.x
                hoverY = mouse.y

                if (!(mouse.buttons & Qt.LeftButton)) {
                    hoveredObjectLabel = skyContext.objectLabelAt(
                        mouse.x,
                        mouse.y,
                        skyViewport.width,
                        skyViewport.height
                    )
                    return
                }

                const deltaX = mouse.x - lastX
                const deltaY = mouse.y - lastY
                skyContext.panViewBy(-deltaX * azimuthSensitivity, -deltaY * altitudeSensitivity)
                lastX = mouse.x
                lastY = mouse.y
                hoveredObjectLabel = ""
            }
            onWheel: function(wheel) {
                skyContext.zoomViewByWheelDelta(wheel.angleDelta.y)
                hoveredObjectLabel = skyContext.objectLabelAt(
                    wheel.x,
                    wheel.y,
                    skyViewport.width,
                    skyViewport.height
                )
                wheel.accepted = true
            }
            onExited: hoveredObjectLabel = ""
        }

        Rectangle {
            id: hoverObjectLabel
            visible: viewPanArea.hoveredObjectLabel.length > 0
            x: Math.min(viewPanArea.hoverX + 14, Math.max(0, parent.width - width - 8))
            y: Math.min(viewPanArea.hoverY + 14, Math.max(0, parent.height - height - 8))
            radius: 6
            color: "#cc0a1222"
            border.width: 1
            border.color: "#6b8fc8"
            z: 10
            width: hoverLabelText.implicitWidth + 14
            height: hoverLabelText.implicitHeight + 8

            Label {
                id: hoverLabelText
                anchors.centerIn: parent
                text: viewPanArea.hoveredObjectLabel
                color: "#e6f0ff"
                font.family: "Avenir Next"
                font.pixelSize: 14
            }
        }

        Repeater {
            model: {
                skyContext.skyContextSummary
                return skyContext.constellationLabels(skyViewport.width, skyViewport.height)
            }

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - (width * 0.5)
                y: modelData.y - height - 8
                visible: !root.overlapsTimelinePanel(x, y, width, height)
                radius: 5
                color: "#aa071328"
                border.width: 1
                border.color: "#78a6d8"
                z: 8
                width: constellationLabelText.implicitWidth + 12
                height: constellationLabelText.implicitHeight + 6

                Label {
                    id: constellationLabelText
                    anchors.centerIn: parent
                    text: modelData.text
                    color: "#c9dcff"
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        Repeater {
            model: [
                { label: "N", azimuthDeg: 0.0, color: "#9ce7ff" },
                { label: "E", azimuthDeg: 90.0, color: "#ffcf9d" },
                { label: "S", azimuthDeg: 180.0, color: "#ffb0b0" },
                { label: "W", azimuthDeg: 270.0, color: "#a8e9c8" }
            ]

            delegate: Rectangle {
                required property var modelData
                readonly property real markerX: {
                    skyContext.viewCenterAltitudeDeg
                    skyContext.viewCenterAzimuthDeg
                    skyContext.projectionTypeText
                    return skyContext.projectedX(0.0, modelData.azimuthDeg, skyViewport.width, skyViewport.height)
                }
                readonly property real markerY: {
                    skyContext.viewCenterAltitudeDeg
                    skyContext.viewCenterAzimuthDeg
                    skyContext.projectionTypeText
                    return skyContext.projectedY(0.0, modelData.azimuthDeg, skyViewport.width, skyViewport.height)
                }

                visible: {
                    skyContext.viewCenterAltitudeDeg
                    skyContext.viewCenterAzimuthDeg
                    skyContext.projectionTypeText
                    return skyContext.isProjectedVisible(0.0, modelData.azimuthDeg, skyViewport.width, skyViewport.height)
                }
                x: markerX - (width * 0.5)
                y: markerY - height - 8
                radius: 6
                color: "#880a1222"
                border.width: 1
                border.color: modelData.color
                z: 9
                width: cardinalSectorLabel.implicitWidth + 12
                height: cardinalSectorLabel.implicitHeight + 6

                Label {
                    id: cardinalSectorLabel
                    anchors.centerIn: parent
                    text: modelData.label
                    color: modelData.color
                    font.family: "Avenir Next"
                    font.bold: true
                    font.pixelSize: 14
                }
            }
        }

        Rectangle {
            id: timelineToolbar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            property bool collapsed: false
            readonly property real expandedWidth: timelineToolbarRow.implicitWidth + 16
            width: collapsed ? 0 : expandedWidth
            height: timelineToolbarRow.implicitHeight + 14
            radius: 10
            color: "#7f0b1428"
            border.width: 1
            border.color: "#335177"
            opacity: collapsed ? 0.0 : 1.0

            Behavior on width {
                NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
            }
            Behavior on opacity {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }

            Row {
                id: timelineToolbarRow
                anchors.centerIn: parent
                spacing: 6
                visible: !timelineToolbar.collapsed
                enabled: !timelineToolbar.collapsed

                property var speedValues: [0.25, 0.5, 1.0, 2.0, 4.0, 8.0]
                property var stepValues: [1, 10, 60, 300, 3600]
                property var magnitudeValues: [2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]

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
                        color: "#e8f4ff"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 8
                        color: parent.down ? "#2a4a72" : (parent.hovered ? "#335b89" : "#1e3d63")
                        border.width: 1
                        border.color: "#75bde8"
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
                        color: "#e8f4ff"
                        font: timelineComboControl.font
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    indicator: Text {
                        text: "\u25BE"
                        color: "#b8daf7"
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                    }

                    background: Rectangle {
                        radius: 8
                        color: timelineComboControl.pressed ? "#2a4a72" : (timelineComboControl.hovered ? "#335b89" : "#1e3d63")
                        border.width: 1
                        border.color: "#75bde8"
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
                            color: "#102745"
                            border.width: 1
                            border.color: "#4f79a8"
                        }
                    }

                    delegate: ItemDelegate {
                        id: timelineComboDelegate
                        width: timelineComboControl.width - 8
                        highlighted: timelineComboControl.highlightedIndex === index

                        contentItem: Text {
                            text: modelData
                            color: highlighted ? "#e8f4ff" : "#bfd6f5"
                            font: timelineComboControl.font
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            radius: 7
                            color: highlighted ? "#2f5f94" : (timelineComboDelegate.hovered ? "#1c3b60" : "transparent")
                        }
                    }
                }

                TimelineToolbarButton {
                    text: skyContext.live ? "Pause" : "Play"
                    onClicked: skyContext.togglePlayPause()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: skyContext.live
                        ? "Pause live timeline updates"
                        : "Resume live timeline updates"
                }
                TimelineToolbarButton {
                    text: "<"
                    onClicked: skyContext.stepBackward()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Step backward by selected interval"
                }
                TimelineToolbarButton {
                    text: ">"
                    onClicked: skyContext.stepForward()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Step forward by selected interval"
                }
                TimelineToolbarCombo {
                    id: speedCombo
                    model: ["0.25x", "0.5x", "1x", "2x", "4x", "8x"]
                    implicitWidth: 78
                    onActivated: skyContext.setSpeedMultiplier(timelineToolbarRow.speedValues[currentIndex])
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Set live timeline speed multiplier"
                }
                TimelineToolbarCombo {
                    id: stepCombo
                    model: ["1s", "10s", "1m", "5m", "1h"]
                    implicitWidth: 74
                    onActivated: skyContext.setStepSeconds(timelineToolbarRow.stepValues[currentIndex])
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Set manual step interval"
                }
                TimelineToolbarButton {
                    text: "Reset"
                    onClicked: skyContext.resetViewDirection()
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Reset view direction to default south-up framing"
                }
                TimelineToolbarCombo {
                    id: magnitudeCombo
                    model: ["2.0", "3.0", "4.0", "5.0", "6.0", "7.0", "8.0"]
                    implicitWidth: 70
                    onActivated: skyContext.setMagnitudeCutoff(timelineToolbarRow.magnitudeValues[currentIndex])
                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: "Set star visual magnitude limit (higher value shows more stars)"
                }
            }
        }

        ToolButton {
            id: timelineToolbarToggle
            anchors.verticalCenter: timelineToolbar.verticalCenter
            anchors.right: timelineToolbar.left
            anchors.rightMargin: 6
            width: 30
            height: 44
            text: timelineToolbar.collapsed ? "\u25B6" : "\u25C0"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            z: 11
            onClicked: timelineToolbar.collapsed = !timelineToolbar.collapsed
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: timelineToolbar.collapsed
                ? "Show timeline panel"
                : "Hide timeline panel"

            contentItem: Text {
                text: timelineToolbarToggle.text
                color: "#eaf7ff"
                font: timelineToolbarToggle.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 8
                color: timelineToolbarToggle.down
                    ? "#27476d"
                    : (timelineToolbarToggle.hovered ? "#315881" : "#1a3352")
                border.width: 1
                border.color: "#6fbde6"
            }
        }

    }
}
