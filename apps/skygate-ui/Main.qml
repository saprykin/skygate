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
    title: "Skygate"

    function syncSettingsFormFromContext() {
        liveCheckBox.checked = skyContext.live
        utcDateInput.text = skyContext.utcDateText
        utcTimeInput.text = skyContext.utcTimeText
        latitudeInput.text = skyContext.latitudeText
        longitudeInput.text = skyContext.longitudeText
        elevationInput.text = skyContext.elevationText
        projectionCombo.currentIndex = Math.max(0, projectionCombo.model.indexOf(skyContext.projectionTypeText))
        catalogPresetCombo.currentIndex = Math.max(
            0,
            Math.min(catalogPresetCombo.count - 1, skyContext.catalogPresetIndex())
        )
        catalogUrlInput.text = skyContext.catalogUrlText()
    }

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

    function normalizeAzimuth(azimuthDeg) {
        let normalized = azimuthDeg % 360.0
        if (normalized < 0.0) {
            normalized += 360.0
        }
        return normalized
    }

    function cardinalLabel(azimuthDeg) {
        const normalized = normalizeAzimuth(azimuthDeg)
        if (normalized >= 315.0 || normalized < 45.0) {
            return "N"
        }
        if (normalized < 135.0) {
            return "E"
        }
        if (normalized < 225.0) {
            return "S"
        }
        return "W"
    }

    function applySettingsFormToContext() {
        skyContext.setUtcDateText(utcDateInput.text)
        skyContext.setUtcTimeText(utcTimeInput.text)
        skyContext.setLatitudeText(latitudeInput.text)
        skyContext.setLongitudeText(longitudeInput.text)
        skyContext.setElevationText(elevationInput.text)
        skyContext.setProjectionTypeText(projectionCombo.currentText)
        skyContext.setCatalogPresetIndex(catalogPresetCombo.currentIndex)
        skyContext.setCatalogUrlText(catalogUrlInput.text)
        skyContext.setLive(liveCheckBox.checked)
        syncSettingsFormFromContext()
    }

    menuBar: MenuBar {
        Menu {
            title: "&App"
            MenuItem {
                text: "&Preferences..."
                onTriggered: {
                    root.syncSettingsFormFromContext()
                    preferencesWindow.visible = true
                    preferencesWindow.raise()
                    preferencesWindow.requestActivate()
                }
            }
        }
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

    Window {
        id: preferencesWindow
        title: "Preferences"
        width: 760
        height: 560
        minimumWidth: 700
        minimumHeight: 520
        visible: false
        transientParent: root
        flags: Qt.Dialog
        modality: Qt.WindowModal

        property color frameBackground: "#0d162c"
        property color panelBackground: "#122243"
        property color sectionBackground: "#0f1d39"
        property color borderColor: "#345984"
        property color textPrimary: "#ecf4ff"
        property color textSecondary: "#9fb8dd"
        property int selectedPage: 0

        component PreferencesTextField : TextField {
            id: textControl
            font.family: "Avenir Next"
            implicitHeight: 36
            color: "#ecf4ff"
            horizontalAlignment: Text.AlignLeft
            placeholderTextColor: "#86a7cf"
            selectedTextColor: "#f4fbff"
            selectionColor: "#4f90cd"
            leftPadding: 10
            rightPadding: 10
            topPadding: 6
            bottomPadding: 6
            background: Rectangle {
                radius: 9
                color: "#102544"
                border.width: 1
                border.color: textControl.activeFocus ? "#8fd9ff" : "#3f648d"
            }
        }

        component PreferencesCheckBox : CheckBox {
            id: checkControl
            implicitWidth: 24
            implicitHeight: 24
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0

            indicator: Rectangle {
                implicitWidth: 24
                implicitHeight: 24
                radius: 5
                color: checkControl.checked ? "#2f79b8" : "#102544"
                border.width: 1
                border.color: checkControl.checked ? "#9de2ff" : "#5c83aa"

                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    color: "#ecf8ff"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    visible: checkControl.checked
                }
            }

            contentItem: Item {}
        }

        component PreferencesComboBox : ComboBox {
            id: comboControl
            font.family: "Avenir Next"
            implicitHeight: 36
            leftPadding: 10
            rightPadding: 30
            topPadding: 5
            bottomPadding: 5

            contentItem: Text {
                text: comboControl.displayText
                color: "#ecf4ff"
                font: comboControl.font
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            indicator: Text {
                text: "\u25BE"
                color: "#a4c5eb"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 10
            }

            background: Rectangle {
                radius: 9
                color: "#102544"
                border.width: 1
                border.color: comboControl.activeFocus ? "#8fd9ff" : "#3f648d"
            }

            popup: Popup {
                y: comboControl.height + 4
                width: comboControl.width
                padding: 4
                implicitHeight: Math.min(contentItem.implicitHeight + 8, 220)

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: comboControl.delegateModel
                    currentIndex: comboControl.highlightedIndex
                }

                background: Rectangle {
                    radius: 10
                    color: "#112949"
                    border.width: 1
                    border.color: "#4e79a8"
                }
            }

            delegate: ItemDelegate {
                id: preferencesComboDelegate
                width: comboControl.width - 8
                highlighted: comboControl.highlightedIndex === index

                contentItem: Text {
                    text: modelData
                    color: highlighted ? "#ecf8ff" : "#bfd6f5"
                    font: comboControl.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: 8
                    color: highlighted ? "#2f5f94" : (preferencesComboDelegate.hovered ? "#1e3d60" : "transparent")
                }
            }
        }

        component PreferencesActionButton : Rectangle {
            id: actionButton
            property string text: ""
            property bool enabled: true
            signal clicked()

            implicitWidth: 160
            implicitHeight: 36
            radius: 9
            color: !enabled
                   ? "#1a2b43"
                   : (actionMouse.pressed ? "#2a4e78" : (actionMouse.containsMouse ? "#356392" : "#2f5a87"))
            border.width: 1
            border.color: enabled ? "#8dcfff" : "#4d6d92"
            opacity: enabled ? 1.0 : 0.65

            Label {
                anchors.centerIn: parent
                text: actionButton.text
                color: "#edf8ff"
                font.family: "Avenir Next"
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: actionMouse
                anchors.fill: parent
                hoverEnabled: actionButton.enabled
                enabled: actionButton.enabled
                cursorShape: actionButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: actionButton.clicked()
            }
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
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#13284a" }
                GradientStop { position: 0.5; color: "#0d1f3a" }
                GradientStop { position: 1.0; color: "#091326" }
            }

            Rectangle {
                width: 320
                height: 320
                radius: 160
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: -120
                anchors.leftMargin: -90
                color: "#4ec8ef22"
            }

            Rectangle {
                width: 220
                height: 220
                radius: 110
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.bottomMargin: -80
                anchors.rightMargin: -50
                color: "#2f82c422"
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 710)
                height: Math.min(parent.height - 40, 530)
                radius: 22
                color: preferencesWindow.frameBackground
                border.width: 1
                border.color: preferencesWindow.borderColor
                opacity: preferencesWindow.visible ? 1.0 : 0.0
                scale: preferencesWindow.visible ? 1.0 : 0.985

                Behavior on opacity {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }
                Behavior on scale {
                    NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: "Preferences"
                                color: preferencesWindow.textPrimary
                                font.family: "Avenir Next"
                                font.pixelSize: 30
                                font.weight: Font.DemiBold
                            }

                            Label {
                                text: "Observer settings and catalog management"
                                color: preferencesWindow.textSecondary
                                font.family: "Avenir Next"
                            }
                        }

                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 16
                        color: preferencesWindow.panelBackground
                        border.width: 1
                        border.color: preferencesWindow.borderColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 170
                                Layout.fillHeight: true
                                radius: 12
                                color: "#0e1d38"
                                border.width: 1
                                border.color: "#2f517a"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    Label {
                                        text: "Sections"
                                        color: preferencesWindow.textSecondary
                                        font.family: "Avenir Next"
                                        font.weight: Font.DemiBold
                                    }

                                    Rectangle {
                                        id: skySectionButton
                                        Layout.fillWidth: true
                                        implicitHeight: 38
                                        radius: 10
                                        readonly property bool active: preferencesWindow.selectedPage === 0
                                        color: active
                                               ? "#2f79b8"
                                               : (skySectionMouse.pressed ? "#213c5b" : (skySectionMouse.containsMouse ? "#1c3452" : "#142943"))
                                        border.width: 1
                                        border.color: active ? "#9de2ff" : "#4f769e"

                                        Label {
                                            anchors.centerIn: parent
                                            text: "Sky"
                                            color: skySectionButton.active ? "#ecf8ff" : "#bdd4f4"
                                            font.family: "Avenir Next"
                                            font.weight: Font.DemiBold
                                        }

                                        MouseArea {
                                            id: skySectionMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: preferencesWindow.selectedPage = 0
                                        }
                                    }

                                    Rectangle {
                                        id: catalogSectionButton
                                        Layout.fillWidth: true
                                        implicitHeight: 38
                                        radius: 10
                                        readonly property bool active: preferencesWindow.selectedPage === 1
                                        color: active
                                               ? "#2f79b8"
                                               : (catalogSectionMouse.pressed ? "#213c5b" : (catalogSectionMouse.containsMouse ? "#1c3452" : "#142943"))
                                        border.width: 1
                                        border.color: active ? "#9de2ff" : "#4f769e"

                                        Label {
                                            anchors.centerIn: parent
                                            text: "Catalog"
                                            color: catalogSectionButton.active ? "#ecf8ff" : "#bdd4f4"
                                            font.family: "Avenir Next"
                                            font.weight: Font.DemiBold
                                        }

                                        MouseArea {
                                            id: catalogSectionMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: preferencesWindow.selectedPage = 1
                                        }
                                    }

                                    Item {
                                        Layout.fillHeight: true
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: preferencesWindow.selectedPage === 0
                                              ? "Observer location and projection"
                                              : "Catalog source and download settings"
                                        color: "#89a8d2"
                                        font.family: "Avenir Next"
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }

                            StackLayout {
                                currentIndex: preferencesWindow.selectedPage
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Item {
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 12
                                        color: preferencesWindow.sectionBackground
                                        border.width: 1
                                        border.color: "#2b4b74"

                                        GridLayout {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            columns: 2
                                            rowSpacing: 8
                                            columnSpacing: 12

                                            Label {
                                                text: "Live updates"
                                                color: "#cad9f7"
                                                font.family: "Avenir Next"
                                            }
                                            PreferencesCheckBox {
                                                id: liveCheckBox
                                            }

                                            Label {
                                                text: "UTC Date"
                                                color: "#cad9f7"
                                                font.family: "Avenir Next"
                                            }
                                            PreferencesTextField {
                                                id: utcDateInput
                                                Layout.fillWidth: true
                                                placeholderText: "YYYY-MM-DD"
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
                                            }

                                            Label {
                                                text: "Latitude"
                                                color: "#cad9f7"
                                                font.family: "Avenir Next"
                                            }
                                            PreferencesTextField {
                                                id: latitudeInput
                                                Layout.fillWidth: true
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
                                                model: ["Stereographic", "AzimuthalEquidistant"]
                                            }
                                        }
                                    }
                                }

                                Item {
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 12
                                        color: preferencesWindow.sectionBackground
                                        border.width: 1
                                        border.color: "#2b4b74"

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 10

                                            Label {
                                                text: "Catalog preset"
                                                color: "#cad9f7"
                                                font.family: "Avenir Next"
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                PreferencesComboBox {
                                                    id: catalogPresetCombo
                                                    Layout.fillWidth: true
                                                    model: [
                                                        "Bundled (recommended)",
                                                        "Starter (bright objects)",
                                                        "Major constellations",
                                                        "HYG v3 stars (Astronexus)"
                                                    ]
                                                }

                                                PreferencesActionButton {
                                                    text: "Use Preset"
                                                    onClicked: {
                                                        if (catalogPresetCombo.currentIndex === 0) {
                                                            skyContext.loadCatalogPreset("bundled")
                                                        } else if (catalogPresetCombo.currentIndex === 1) {
                                                            skyContext.loadCatalogPreset("starter")
                                                        } else if (catalogPresetCombo.currentIndex === 2) {
                                                            skyContext.loadCatalogPreset("constellations_major")
                                                        } else {
                                                            skyContext.loadCatalogPreset("hyg_v3")
                                                            catalogUrlInput.text = "https://raw.githubusercontent.com/astronexus/HYG-Database/master/hygdata_v3.csv"
                                                        }
                                                    }
                                                }
                                            }

                                            Label {
                                                text: "Catalog URL"
                                                color: "#cad9f7"
                                                font.family: "Avenir Next"
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                PreferencesTextField {
                                                    id: catalogUrlInput
                                                    Layout.fillWidth: true
                                                    text: "https://astronexus.com/downloads/catalogs/hygdata_v42.csv.gz"
                                                    placeholderText: "https://example.com/skygate-catalog.txt or HYG CSV URL"
                                                    Component.onCompleted: cursorPosition = 0
                                                    onActiveFocusChanged: {
                                                        if (!activeFocus) {
                                                            cursorPosition = 0
                                                        }
                                                    }
                                                    onTextChanged: {
                                                        if (!activeFocus) {
                                                            cursorPosition = 0
                                                        }
                                                    }
                                                }

                                                PreferencesActionButton {
                                                    text: skyContext.downloadingCatalog ? "Downloading..." : "Download"
                                                    enabled: !skyContext.downloadingCatalog
                                                    onClicked: skyContext.downloadCatalogFromUrl(catalogUrlInput.text)
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: false
                                                Layout.preferredHeight: 82
                                                Layout.minimumHeight: 82
                                                radius: 9
                                                color: "#0c1830"
                                                border.width: 1
                                                border.color: "#2b4a72"

                                                Label {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    text: skyContext.catalogStatusText
                                                    color: "#9ab0d6"
                                                    font.pixelSize: 12
                                                    font.family: "Avenir Next"
                                                    wrapMode: Text.Wrap
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        Layout.minimumHeight: 52
                        implicitHeight: 52
                        radius: 12
                        color: "#0f1c35"
                        border.width: 1
                        border.color: "#2f5078"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Item {
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                id: applyButton
                                Layout.preferredWidth: 112
                                Layout.fillHeight: true
                                radius: 10
                                color: applyMouse.pressed ? "#296fa9" : (applyMouse.containsMouse ? "#378ac8" : "#307fbf")
                                border.width: 1
                                border.color: "#b9ecff"

                                Label {
                                    anchors.centerIn: parent
                                    text: "Apply"
                                    color: "#f4fbff"
                                    font.family: "Avenir Next"
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    id: applyMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.applySettingsFormToContext()
                                }
                            }
                        }
                    }
                }

                ToolButton {
                    id: closeIconButton
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 12
                    anchors.rightMargin: 12
                    width: 34
                    height: 34
                    text: "\u2715"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    onClicked: preferencesWindow.visible = false
                    ToolTip.visible: hovered
                    ToolTip.text: "Close"

                    contentItem: Text {
                        text: closeIconButton.text
                        color: "#eaf7ff"
                        font: closeIconButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 17
                        color: closeIconButton.down ? "#27476d" : (closeIconButton.hovered ? "#315881" : "#1a3352")
                        border.width: 1
                        border.color: "#6fbde6"
                    }
                }
            }
        }
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

        Label {
            id: horizonDirectionLabel
            readonly property real viewAltitudeDeg: skyContext.viewCenterAltitudeDeg
            readonly property real viewAzimuthDeg: skyContext.viewCenterAzimuthDeg
            readonly property string projectionState: skyContext.skyContextSummary
            readonly property real markerX: {
                projectionState
                return skyContext.projectedX(0.0, viewAzimuthDeg, skyViewport.width, skyViewport.height)
            }
            readonly property real markerY: {
                projectionState
                return skyContext.projectedY(0.0, viewAzimuthDeg, skyViewport.width, skyViewport.height)
            }

            text: root.cardinalLabel(viewAzimuthDeg)
            color: "#c8ddff"
            font.family: "Avenir Next"
            font.bold: true
            visible: {
                projectionState
                return skyContext.isProjectedVisible(0.0, viewAzimuthDeg, skyViewport.width, skyViewport.height)
            }
            x: markerX - (implicitWidth * 0.5)
            y: markerY - implicitHeight - 6
        }

        Rectangle {
            id: timelineToolbar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            width: timelineToolbarRow.implicitWidth + 16
            height: timelineToolbarRow.implicitHeight + 14
            radius: 10
            color: "#7f0b1428"
            border.width: 1
            border.color: "#335177"

            Row {
                id: timelineToolbarRow
                anchors.centerIn: parent
                spacing: 6

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

    }
}
