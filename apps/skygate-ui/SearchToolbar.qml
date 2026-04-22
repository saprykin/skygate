import QtQuick
import QtQuick.Controls

Item {
    id: toolbarRoot
    required property var skyContextController
    property var onRequestExpand: null
    property alias panelItem: searchToolbarPanel
    property alias toggleItem: searchToolbarToggle
    property alias dropdownItem: resultsDropdown
    readonly property var searchModel: skyContextController.objectSearchModel
    readonly property real controlHeight: 38
    readonly property real expandedTotalWidth: searchToolbarPanel.expandedWidth
        + searchToolbarToggle.implicitWidth + 6
    property bool suppressFilterSync: false
    z: 20

    function clearSearch() {
        searchField.text = ""
        if (searchModel) {
            searchModel.filterText = ""
        }
        resultsListView.currentIndex = -1
        skyContextController.clearSelectedSearchTarget()
    }

    function activateTarget(displayText, targetKind, targetId) {
        if (!skyContextController.focusSearchTarget(targetKind, targetId)) {
            return
        }

        suppressFilterSync = true
        searchField.text = displayText
        suppressFilterSync = false
        if (searchModel) {
            searchModel.filterText = ""
        }
        resultsListView.currentIndex = -1
        searchField.focus = false
    }

    function activateCurrentResult() {
        if (!resultsDropdown.visible || resultsListView.count <= 0 || !resultsListView.currentItem) {
            return
        }

        resultsListView.currentItem.activate()
    }

    implicitWidth: searchToolbarPanel.expandedWidth + searchToolbarToggle.implicitWidth + 6
    implicitHeight: Math.max(searchToolbarPanel.implicitHeight, searchToolbarToggle.implicitHeight)

    Connections {
        target: skyContextController

        function onSearchToolbarCollapsedChanged() {
            if (skyContextController.searchToolbarCollapsed) {
                toolbarRoot.clearSearch()
            }
        }
    }

    Rectangle {
        id: searchToolbarPanel
        anchors.top: parent.top
        anchors.left: parent.left
        property bool collapsed: skyContextController.searchToolbarCollapsed
        readonly property real expandedWidth: searchToolbarRow.implicitWidth + 16
        width: collapsed ? 0 : expandedWidth
        height: searchToolbarRow.implicitHeight + 14
        implicitHeight: height
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
            id: searchToolbarRow
            anchors.centerIn: parent
            spacing: 6
            visible: !searchToolbarPanel.collapsed
            enabled: !searchToolbarPanel.collapsed

            PreferencesTextField {
                id: searchField
                width: 420
                implicitHeight: toolbarRoot.controlHeight
                placeholderText: "Search planets, stars, HIP, constellations"
                onTextChanged: {
                    if (!toolbarRoot.suppressFilterSync && toolbarRoot.searchModel) {
                        toolbarRoot.searchModel.filterText = text
                    }
                    if (!toolbarRoot.suppressFilterSync && text.trim().length === 0) {
                        skyContextController.clearSelectedSearchTarget()
                    }
                }
                onActiveFocusChanged: {
                    if (!toolbarRoot.searchModel) {
                        return
                    }

                    if (activeFocus) {
                        toolbarRoot.searchModel.filterText = text
                    } else if (toolbarRoot.searchModel.filterText === text) {
                        toolbarRoot.searchModel.filterText = ""
                    }
                }
                onAccepted: toolbarRoot.activateCurrentResult()
                Keys.onEscapePressed: function(event) {
                    toolbarRoot.clearSearch()
                    searchField.focus = false
                    event.accepted = true
                }
            }

            Button {
                id: clearButton
                visible: searchField.text.length > 0
                enabled: visible
                text: "✕"
                font.family: "Avenir Next"
                font.weight: Font.DemiBold
                implicitHeight: toolbarRoot.controlHeight
                implicitWidth: toolbarRoot.controlHeight
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                onClicked: {
                    toolbarRoot.clearSearch()
                    searchField.forceActiveFocus()
                }

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
        }
    }

    ToolButton {
        id: searchToolbarToggle
        anchors.verticalCenter: searchToolbarPanel.verticalCenter
        anchors.left: searchToolbarPanel.right
        anchors.leftMargin: 6
        width: 30
        height: 44
        implicitWidth: width
        implicitHeight: height
        text: searchToolbarPanel.collapsed ? "\u25C0" : "\u25B6"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        z: 11
        onClicked: {
            if (skyContextController.searchToolbarCollapsed && toolbarRoot.onRequestExpand) {
                toolbarRoot.onRequestExpand()
            }

            skyContextController.setSearchToolbarCollapsed(
                !skyContextController.searchToolbarCollapsed
            )
        }
        ToolTip.visible: hovered
        ToolTip.delay: 250
        ToolTip.text: searchToolbarPanel.collapsed
            ? "Show search panel"
            : "Hide search panel"

        contentItem: Text {
            text: searchToolbarToggle.text
            color: "#e8f4ff"
            font: searchToolbarToggle.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 8
            color: searchToolbarToggle.down
                ? "#27476d"
                : (searchToolbarToggle.hovered ? "#315881" : "#1a3352")
            border.width: 1
            border.color: "#6fbde6"
        }
    }

    Rectangle {
        id: resultsDropdown
        anchors.top: searchToolbarPanel.bottom
        anchors.topMargin: 6
        anchors.left: searchToolbarPanel.left
        width: Math.max(300, searchToolbarPanel.width)
        visible: !searchToolbarPanel.collapsed
            && searchField.activeFocus
            && searchField.text.trim().length > 0
        height: Math.min(
            260,
            Math.max(
                emptyLabel.implicitHeight + 20,
                resultsListView.count > 0
                    ? resultsListView.contentHeight + (resultsListView.anchors.margins * 2) + 2
                    : emptyLabel.implicitHeight + 20
            )
        )
        radius: 10
        color: "#102745"
        border.width: 1
        border.color: "#4f79a8"
        z: 30
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "#102745"
        }

        ListView {
            id: resultsListView
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            model: toolbarRoot.searchModel
            spacing: 4
            onCountChanged: currentIndex = count > 0 ? 0 : -1

            delegate: Rectangle {
                id: resultDelegate
                required property string displayText
                required property string detailText
                required property string targetKind
                required property string targetId
                required property bool selectable
                required property int index

                function activate() {
                    toolbarRoot.activateTarget(displayText, targetKind, targetId)
                }

                width: resultsListView.width
                height: 46
                radius: 8
                color: resultsListView.currentIndex === resultDelegate.index
                    ? "#2f5f94"
                    : (delegateMouse.containsMouse ? "#1c3b60" : "transparent")

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: displayText
                        color: "#e8f4ff"
                        font.family: "Avenir Next"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        text: detailText
                        color: "#bfd6f5"
                        font.family: "Avenir Next"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: delegateMouse
                    anchors.fill: parent
                    enabled: selectable
                    hoverEnabled: selectable
                    cursorShape: selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onEntered: resultsListView.currentIndex = resultDelegate.index
                    onClicked: resultDelegate.activate()
                }
            }
        }

        Label {
            id: emptyLabel
            anchors.centerIn: parent
            visible: resultsListView.count === 0
            text: "No matching objects"
            color: "#bfd6f5"
            font.family: "Avenir Next"
            font.pixelSize: 11
        }
    }
}
